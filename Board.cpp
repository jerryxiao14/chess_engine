#include <iostream>
#include <cstdint>
#include <bit>
#include <cassert>
#include <cctype>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <string.h>


typedef uint64_t U64;


enum Color {WHITE,BLACK,BOTH};
enum Piece {P,N,B,R,Q,K};


enum Square {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, NO_SQ
};

const int WKCA = 1;
const int WQCA = 2;
const int BKCA = 4;
const int BQCA = 8;

inline void set_bit(U64 &bitboard,int square){
    bitboard |= (1ULL<<square);
}

inline int get_bit(U64 &bitboard,int square){
    return (bitboard >> square) & 1ULL;
}

inline void pop_bit(U64 &bitboard, int square){
    bitboard &= ~(1ULL<<square);
}

inline int pop_lsb(U64 &bb){
    int square = __builtin_ctzll(bb);
    bb &= bb-1;
    return square;
}

inline int get_lsb(U64 bb){
    return __builtin_ctzll(bb);
}


struct Board{
    // bitboards for whether a piece exists on a square for every piece of every clor
    U64 pieces[2][6];

    U64 occupancies[3];

    Color sideToMove;

    int castlingRights;

    Square enPassant;
    
    int halfmoveClock;
    
    int fullmoveNumber;

    U64 zobristKey;
};

// state needed to reverse make_move that isn't recoverable from the move
// encoding alone (the encoding tells us a capture happened, not which piece)
struct Undo{
    int capturedPiece;
    int castlingRights;
    Square enPassant;
    int halfmoveClock;
    int fullmoveNumber;
    U64 zobristKey;
};

struct Move{

    /*
    Encoidng:
    bits 0-5: from square
    bits 6-11: to square 
    bits 12 -15: piece 
    bits 16-19: promoted piec
    bit 20: capture 
    bit 21: double pawn push
    bit 22: en passant
    bit 23: castling
    */
    uint32_t move;
};
// bitboard visualization



// encoding 
inline uint32_t encode_move(int from, int to, int piece, int promoted, int capture, int double_push, int enpassant, int castling){
    return 
        from|
        (to<<6)|
        (piece<<12)|
        (promoted<<16)|
        (capture<<20)|
        (double_push<<21)|
        (enpassant<<22)|
        (castling<<23);
}

// extractors

#define FROM(move) ((move)&0x3f)
#define TO(move) ((move>>6)&0x3f)
#define PIECE(move) (((move)>>12) & 0xf)

// Zobrist hashing
U64 piece_keys[2][6][64];
U64 side_key;
U64 castle_right_key[4]; // one key per WKCA/WQCA/BKCA/BQCA bit
U64 castling_keys[16];   // XOR of castle_right_key[] for each combination of rights
U64 en_passant_keys[8];  // keyed by file, since only the file matters for ep availability

void init_zobrist(){
    std::mt19937_64 rng(0xC0FFEE123456789ULL);

    for(int c = WHITE; c <= BLACK; c++)
        for(int p = P; p <= K; p++)
            for(int sq = 0; sq < 64; sq++)
                piece_keys[c][p][sq] = rng();

    side_key = rng();

    for(int i = 0; i < 4; i++) castle_right_key[i] = rng();
    for(int i = 0; i < 16; i++){
        castling_keys[i] = 0;
        if(i & WKCA) castling_keys[i] ^= castle_right_key[0];
        if(i & WQCA) castling_keys[i] ^= castle_right_key[1];
        if(i & BKCA) castling_keys[i] ^= castle_right_key[2];
        if(i & BQCA) castling_keys[i] ^= castle_right_key[3];
    }

    for(int f = 0; f < 8; f++) en_passant_keys[f] = rng();
}

inline U64 ep_zobrist(Square sq){
    return (sq == NO_SQ) ? 0ULL : en_passant_keys[sq % 8];
}

U64 compute_zobrist_key(const Board &board){
    U64 key = 0;

    for(int c = WHITE; c <= BLACK; c++){
        for(int p = P; p <= K; p++){
            U64 bb = board.pieces[c][p];
            while(bb){
                int sq = pop_lsb(bb);
                key ^= piece_keys[c][p][sq];
            }
        }
    }

    if(board.sideToMove == BLACK) key ^= side_key;
    key ^= castling_keys[board.castlingRights];
    key ^= ep_zobrist(board.enPassant);

    return key;
}

Undo make_move(Board &board, uint32_t move){
    int from = FROM(move);
    int to = TO(move);
    int piece = PIECE(move);

    int promoted = (move >> 16)&0xf;

    bool capture = (move>>20)&1;
    bool double_push = (move>>21) &1;
    bool enpassant = (move>>22) & 1;
    bool castling = (move >>23 &1);
    int color = board.sideToMove;
    int us = color;
    int them = color ^1;

    Undo undo;
    undo.capturedPiece = -1;
    undo.castlingRights = board.castlingRights;
    undo.enPassant = board.enPassant;
    undo.halfmoveClock = board.halfmoveClock;
    undo.fullmoveNumber = board.fullmoveNumber;
    undo.zobristKey = board.zobristKey;

    int enpassant_capture_sq = -1;
    // clear moving piece from square "from"
    board.pieces[us][piece] &= ~(1ULL << from);
    board.occupancies[us] &= ~(1ULL << from);
    board.occupancies[BOTH] &= ~(1ULL << from);

    // handle capture
    if(capture){
        if(enpassant){
            if(color==WHITE) enpassant_capture_sq = to+8;
            else enpassant_capture_sq = to-8;

            board.pieces[them][P] &= ~(1ULL<<enpassant_capture_sq);
            board.occupancies[them] &= ~(1ULL<<enpassant_capture_sq);
            board.occupancies[BOTH] &= ~(1ULL << enpassant_capture_sq);
        }
        else{
            for(int pt = P;pt<=K;pt++){
                if(board.pieces[them][pt] & (1ULL<<to)){
                    undo.capturedPiece = pt;
                    board.pieces[them][pt] &= ~(1ULL<<to);
                    board.occupancies[them] &= ~(1ULL <<to);
                    break;
                }
            }
        }

        if(them==WHITE){
            if(to==h1) board.castlingRights &= ~WKCA;
            else if(to==a1) board.castlingRights &= ~WQCA;
        }
        else{
            if(to==h8) board.castlingRights &= ~BKCA;
            else if (to== a8) board.castlingRights &= ~BQCA;
        }
    }

    // plac moved piece on to
    if(promoted){
        board.pieces[us][promoted] |= (1ULL <<to);
    }
    else{
        board.pieces[us][piece] |= (1ULL << to);
    }

    board.occupancies[us] |= (1ULL<<to);
    board.occupancies[BOTH] |= (1ULL<<to);

    if(double_push){
        if(color==WHITE) board.enPassant = Square(to+8);
        else board.enPassant = Square(to-8);
    }
    else board.enPassant = NO_SQ;

    // handle moving rook too for castling
    if(castling){
        // white kingside castling
        if(to==g1){
            board.pieces[us][R] &= ~(1ULL<<(h1));
            board.occupancies[us] &= ~(1ULL<<(h1));
            board.occupancies[BOTH] &= ~(1ULL<<(h1));

            board.pieces[us][R] |= (1ULL<<(f1));
            board.occupancies[us] |= (1ULL<<(f1));
            board.occupancies[BOTH] |= (1ULL<<(f1));
        }
        // white queenside castling
        else if(to==c1){
            board.pieces[us][R] &= ~(1ULL<<(a1));
            board.occupancies[us] &= ~(1ULL<<(a1));
            board.occupancies[BOTH] &= ~(1ULL<<(a1));

            board.pieces[us][R] |= (1ULL<<(d1));
            board.occupancies[us] |= (1ULL<<(d1));
            board.occupancies[BOTH] |= (1ULL<<(d1));
        }
        else if(to==g8){
            board.pieces[us][R] &= ~(1ULL<<(h8));
            board.occupancies[us] &= ~(1ULL<<(h8));
            board.occupancies[BOTH] &= ~(1ULL<<(h8));

            board.pieces[us][R] |= (1ULL<<(f8));
            board.occupancies[us] |= (1ULL<<(f8));
            board.occupancies[BOTH] |= (1ULL<<(f8));
        }
        else{
            board.pieces[us][R] &= ~(1ULL<<(a8));
            board.occupancies[us] &= ~(1ULL<<(a8));
            board.occupancies[BOTH] &= ~(1ULL<<(a8));

            board.pieces[us][R] |= (1ULL<<(d8));
            board.occupancies[us] |= (1ULL<<(d8));
            board.occupancies[BOTH] |= (1ULL<<(d8));
        }
    }

    // castling rights
    if(piece==K){
        if(us==WHITE) board.castlingRights &= ~(WKCA | WQCA);
        else board.castlingRights &= ~(BKCA | BQCA);
    }

    if(piece==R){
        if(us==WHITE){
            if(from==h1) board.castlingRights &= ~WKCA;
            else if(from==a1) board.castlingRights &= ~WQCA;
        }
        else{
            if(from==h8) board.castlingRights &= ~BKCA;
            else if(from==a8) board.castlingRights &= ~BQCA;
        }
    }



    if(piece==P || capture) board.halfmoveClock=0;
    else board.halfmoveClock++;

    if(color==BLACK) board.fullmoveNumber++;

    board.sideToMove = (Color)them;

    // incremental zobrist update
    board.zobristKey ^= piece_keys[us][piece][from];
    board.zobristKey ^= piece_keys[us][promoted ? promoted : piece][to];

    if(capture){
        if(enpassant) board.zobristKey ^= piece_keys[them][P][enpassant_capture_sq];
        else board.zobristKey ^= piece_keys[them][undo.capturedPiece][to];
    }

    if(castling){
        if(to==g1) board.zobristKey ^= piece_keys[us][R][h1] ^ piece_keys[us][R][f1];
        else if(to==c1) board.zobristKey ^= piece_keys[us][R][a1] ^ piece_keys[us][R][d1];
        else if(to==g8) board.zobristKey ^= piece_keys[us][R][h8] ^ piece_keys[us][R][f8];
        else board.zobristKey ^= piece_keys[us][R][a8] ^ piece_keys[us][R][d8];
    }

    board.zobristKey ^= castling_keys[undo.castlingRights] ^ castling_keys[board.castlingRights];
    board.zobristKey ^= ep_zobrist(undo.enPassant) ^ ep_zobrist(board.enPassant);
    board.zobristKey ^= side_key;

    return undo;
}

void unmake_move(Board &board, uint32_t move, const Undo &undo){
    int from = FROM(move);
    int to = TO(move);
    int piece = PIECE(move);

    int promoted = (move >> 16)&0xf;

    bool capture = (move>>20)&1;
    bool enpassant = (move>>22) & 1;
    bool castling = (move >>23 &1);

    board.sideToMove = (Color)(board.sideToMove ^ 1);
    int us = board.sideToMove;
    int them = us ^ 1;

    // undo castling rook move
    if(castling){
        if(to==g1){
            board.pieces[us][R] &= ~(1ULL<<f1);
            board.occupancies[us] &= ~(1ULL<<f1);
            board.occupancies[BOTH] &= ~(1ULL<<f1);

            board.pieces[us][R] |= (1ULL<<h1);
            board.occupancies[us] |= (1ULL<<h1);
            board.occupancies[BOTH] |= (1ULL<<h1);
        }
        else if(to==c1){
            board.pieces[us][R] &= ~(1ULL<<d1);
            board.occupancies[us] &= ~(1ULL<<d1);
            board.occupancies[BOTH] &= ~(1ULL<<d1);

            board.pieces[us][R] |= (1ULL<<a1);
            board.occupancies[us] |= (1ULL<<a1);
            board.occupancies[BOTH] |= (1ULL<<a1);
        }
        else if(to==g8){
            board.pieces[us][R] &= ~(1ULL<<f8);
            board.occupancies[us] &= ~(1ULL<<f8);
            board.occupancies[BOTH] &= ~(1ULL<<f8);

            board.pieces[us][R] |= (1ULL<<h8);
            board.occupancies[us] |= (1ULL<<h8);
            board.occupancies[BOTH] |= (1ULL<<h8);
        }
        else{
            board.pieces[us][R] &= ~(1ULL<<d8);
            board.occupancies[us] &= ~(1ULL<<d8);
            board.occupancies[BOTH] &= ~(1ULL<<d8);

            board.pieces[us][R] |= (1ULL<<a8);
            board.occupancies[us] |= (1ULL<<a8);
            board.occupancies[BOTH] |= (1ULL<<a8);
        }
    }

    // remove the moved (or promoted) piece from "to"
    if(promoted){
        board.pieces[us][promoted] &= ~(1ULL<<to);
    }
    else{
        board.pieces[us][piece] &= ~(1ULL<<to);
    }
    board.occupancies[us] &= ~(1ULL<<to);
    board.occupancies[BOTH] &= ~(1ULL<<to);

    // restore whatever was captured
    if(capture){
        if(enpassant){
            int cap_sq = (us==WHITE) ? to+8 : to-8;
            board.pieces[them][P] |= (1ULL<<cap_sq);
            board.occupancies[them] |= (1ULL<<cap_sq);
            board.occupancies[BOTH] |= (1ULL<<cap_sq);
        }
        else{
            board.pieces[them][undo.capturedPiece] |= (1ULL<<to);
            board.occupancies[them] |= (1ULL<<to);
            board.occupancies[BOTH] |= (1ULL<<to);
        }
    }

    // put the moving piece back on "from"
    board.pieces[us][piece] |= (1ULL<<from);
    board.occupancies[us] |= (1ULL<<from);
    board.occupancies[BOTH] |= (1ULL<<from);

    board.castlingRights = undo.castlingRights;
    board.enPassant = undo.enPassant;
    board.halfmoveClock = undo.halfmoveClock;
    board.fullmoveNumber = undo.fullmoveNumber;
    board.zobristKey = undo.zobristKey;
}

// print bitboard
void print_bitboard(U64 bitboard)
{
    printf("\n");
    
    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // init board square
            int square = rank * 8 + file;
            
            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);
            
            // print bit indexed by board square
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }
        
        printf("\n");
    }
    
    // print files
    printf("\n     a b c d e f g h\n\n");
    
    // print bitboard as decimal
    printf("     bitboard: %llud\n\n", bitboard);
}





U64 knight_attacks[64];
U64 king_attacks[64];
U64 pawn_attacks[2][64];

U64 rook_masks[64];
U64 bishop_masks[64];

int rook_relevant_bits[64];
int bishop_relevant_bits[64];

U64 rook_attacks[64][4096];
U64 bishop_attacks[64][512];


const U64 NOT_A_FILE =
    0xfefefefefefefefeULL;

const U64 NOT_H_FILE =
    0x7f7f7f7f7f7f7f7fULL;

const U64 NOT_AB_FILE =
    0xfcfcfcfcfcfcfcfcULL;

const U64 NOT_GH_FILE =
    0x3f3f3f3f3f3f3f3fULL;

U64 mask_knight_attacks(int square)
{
    U64 attacks = 0ULL;

    U64 knight = 1ULL << square;

    attacks |= (knight << 17) & NOT_A_FILE;
    attacks |= (knight << 15) & NOT_H_FILE;

    attacks |= (knight << 10) & NOT_AB_FILE;
    attacks |= (knight << 6)  & NOT_GH_FILE;

    attacks |= (knight >> 17) & NOT_H_FILE;
    attacks |= (knight >> 15) & NOT_A_FILE;

    attacks |= (knight >> 10) & NOT_GH_FILE;
    attacks |= (knight >> 6)  & NOT_AB_FILE;

    return attacks;
}
void init_knight_attacks(){
    for(int sq = 0;sq<64;sq++){
        knight_attacks[sq] = mask_knight_attacks(sq);   
    }
}

U64 mask_king_attacks(int square)
{
    U64 attacks = 0ULL;

    U64 king = 1ULL << square;

    attacks |= king << 8;
    attacks |= king >> 8;

    attacks |= (king << 1) & NOT_A_FILE;
    attacks |= (king >> 1) & NOT_H_FILE;

    attacks |= (king << 9) & NOT_A_FILE;
    attacks |= (king << 7) & NOT_H_FILE;

    attacks |= (king >> 7) & NOT_A_FILE;
    attacks |= (king >> 9) & NOT_H_FILE;

    return attacks;
}

void init_king_attacks(){
    for(int sq=0;sq<64;sq++) king_attacks[sq]=mask_king_attacks(sq);
}

U64 mask_white_pawn_attacks(int square)
{
    U64 attacks = 0ULL;

    U64 pawn = 1ULL << square;

    attacks |= (pawn >> 7) & NOT_A_FILE;
    attacks |= (pawn >> 9) & NOT_H_FILE;

    return attacks;
}

U64 mask_black_pawn_attacks(int square)
{
    U64 attacks = 0ULL;

    U64 pawn = 1ULL << square;

    attacks |= (pawn << 7) & NOT_H_FILE;
    attacks |= (pawn << 9) & NOT_A_FILE;

    return attacks;
}

void init_pawn_attacks(){
    for(int sq=0;sq<64;sq++){
        pawn_attacks[WHITE][sq]=mask_white_pawn_attacks(sq);
        
        pawn_attacks[BLACK][sq]=mask_black_pawn_attacks(sq);
    }
}

U64 mask_rook_attacks(int square)
{
    U64 attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // north
    for(int r = rank + 1; r <= 6; r++)
        attacks |= (1ULL << (r * 8 + file));

    // south
    for(int r = rank - 1; r >= 1; r--)
        attacks |= (1ULL << (r * 8 + file));

    // east
    for(int f = file + 1; f <= 6; f++)
        attacks |= (1ULL << (rank * 8 + f));

    // west
    for(int f = file - 1; f >= 1; f--)
        attacks |= (1ULL << (rank * 8 + f));

    return attacks;
}

U64 mask_bishop_attacks(int square)
{
    U64 attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // northeast
    for(int r = rank + 1, f = file + 1;
        r <= 6 && f <= 6;
        r++, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
    }

    // northwest
    for(int r = rank + 1, f = file - 1;
        r <= 6 && f >= 1;
        r++, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
    }

    // southeast
    for(int r = rank - 1, f = file + 1;
        r >= 1 && f <= 6;
        r--, f++)
    {
        attacks |= (1ULL << (r * 8 + f));
    }

    // southwest
    for(int r = rank - 1, f = file - 1;
        r >= 1 && f >= 1;
        r--, f--)
    {
        attacks |= (1ULL << (r * 8 + f));
    }

    return attacks;
}


void init_slider_masks(){
    for(int sq=0;sq<64;sq++){
        bishop_masks[sq] = mask_bishop_attacks(sq);

        rook_masks[sq]=mask_rook_attacks(sq);
    }
}

void initialize_relevancy_bits(){
    for(int sq=0;sq<64;sq++){
        rook_relevant_bits[sq]= __builtin_popcountll(rook_masks[sq]);
        bishop_relevant_bits[sq]=__builtin_popcountll(bishop_masks[sq]);
    }
}



U64 set_occupancy(int index, int bits, U64 attack_mask){
    U64 occupancy = 0ULL;
    for(int cnt = 0;cnt<bits;cnt++){
        int square = pop_lsb(attack_mask);

        if(index & (1<<cnt)){
            occupancy |= (1ULL<<square);
        }
    }

    return occupancy;
}

U64 rook_attacks_otf(int square, U64 blockers){
    U64 attacks = 0ULL;
    
    int rank = square/8;
    int file = square %8;

    // north
    for(int r=rank-1;r>=0;r--){
        int sq = r*8+file;
        attacks |= (1ULL<<sq);
        if(blockers&(1ULL<<sq)) break;
    }
    
    // south
    for(int r = rank+1;r<8;r++){
        int sq = r*8+file;
        attacks|=(1ULL<<sq);

        if(blockers&(1ULL<<sq)) break;
    }

    // west
    for(int f = file-1;f>=0;f--){
        int sq = rank*8+f;

        attacks|=(1ULL<<sq);
        if(blockers & (1ULL<<sq)) break;
    }

    // east
    for(int f=file+1;f<8;f++){
        int sq = rank*8+f;
        attacks|=(1ULL<<sq);

        if(blockers & (1ULL << sq)) break;
    }

    return attacks;
}

U64 bishop_attacks_otf(int square, U64 blockers){
    U64 attacks = 0ULL;

    int rank = square/8;
    int file = square %8;

    for(int r=rank-1,f=file-1;r>=0&&f>=0;r--,f--){
        int sq = r*8+f;

        attacks|=(1ULL<<sq);

        if(blockers&(1ULL<<sq)) break;
    }

    for(int r=rank-1,f=file+1;r>=0&&f<8;r--,f++){
        int sq = r*8+f;

        attacks|=(1ULL<<sq);

        if(blockers&(1ULL<<sq)) break;
    }
    for(int r=rank+1,f=file-1;r<8&&f>=0;r++,f--){
        int sq = r*8+f;

        attacks|=(1ULL<<sq);

        if(blockers&(1ULL<<sq)) break;
    }

    for(int r=rank+1,f=file+1;r<8&&f<8;r++,f++){
        int sq = r*8+f;

        attacks|=(1ULL<<sq);

        if(blockers&(1ULL<<sq)) break;
    }

    return attacks;
}

const U64 rook_magics[64] = {
    0x8a80104000800020ULL,
    0x140002000100040ULL,
    0x2801880a0017001ULL,
    0x100081001000420ULL,
    0x200020010080420ULL,
    0x3001c0002010008ULL,
    0x8480008002000100ULL,
    0x2080088004402900ULL,
    0x800098204000ULL,
    0x2024401000200040ULL,
    0x100802000801000ULL,
    0x120800800801000ULL,
    0x208808088000400ULL,
    0x2802200800400ULL,
    0x2200800100020080ULL,
    0x801000060821100ULL,
    0x80044006422000ULL,
    0x100808020004000ULL,
    0x12108a0010204200ULL,
    0x140848010000802ULL,
    0x481828014002800ULL,
    0x8094004002004100ULL,
    0x4010040010010802ULL,
    0x20008806104ULL,
    0x100400080208000ULL,
    0x2040002120081000ULL,
    0x21200680100081ULL,
    0x20100080080080ULL,
    0x2000a00200410ULL,
    0x20080800400ULL,
    0x80088400100102ULL,
    0x80004600042881ULL,
    0x4040008040800020ULL,
    0x440003000200801ULL,
    0x4200011004500ULL,
    0x188020010100100ULL,
    0x14800401802800ULL,
    0x2080040080800200ULL,
    0x124080204001001ULL,
    0x200046502000484ULL,
    0x480400080088020ULL,
    0x1000422010034000ULL,
    0x30200100110040ULL,
    0x100021010009ULL,
    0x2002080100110004ULL,
    0x202008004008002ULL,
    0x20020004010100ULL,
    0x2048440040820001ULL,
    0x101002200408200ULL,
    0x40802000401080ULL,
    0x4008142004410100ULL,
    0x2060820c0120200ULL,
    0x1001004080100ULL,
    0x20c020080040080ULL,
    0x2935610830022400ULL,
    0x44440041009200ULL,
    0x280001040802101ULL,
    0x2100190040002085ULL,
    0x80c0084100102001ULL,
    0x4024081001000421ULL,
    0x20030a0244872ULL,
    0x12001008414402ULL,
    0x2006104900a0804ULL,
    0x1004081002402ULL,
};

const U64 bishop_magics[64] = {
    0x40040844404084ULL,
    0x2004208a004208ULL,
    0x10190041080202ULL,
    0x108060845042010ULL,
    0x581104180800210ULL,
    0x2112080446200010ULL,
    0x1080820820060210ULL,
    0x3c0808410220200ULL,
    0x4050404440404ULL,
    0x21001420088ULL,
    0x24d0080801082102ULL,
    0x1020a0a020400ULL,
    0x40308200402ULL,
    0x4011002100800ULL,
    0x401484104104005ULL,
    0x801010402020200ULL,
    0x400210c3880100ULL,
    0x404022024108200ULL,
    0x810018200204102ULL,
    0x4002801a02003ULL,
    0x85040820080400ULL,
    0x810102c808880400ULL,
    0xe900410884800ULL,
    0x8002020480840102ULL,
    0x220200865090201ULL,
    0x2010100a02021202ULL,
    0x152048408022401ULL,
    0x20080002081110ULL,
    0x4001001021004000ULL,
    0x800040400a011002ULL,
    0xe4004081011002ULL,
    0x1c004001012080ULL,
    0x8004200962a00220ULL,
    0x8422100208500202ULL,
    0x2000402200300c08ULL,
    0x8646020080080080ULL,
    0x80020a0200100808ULL,
    0x2010004880111000ULL,
    0x623000a080011400ULL,
    0x42008c0340209202ULL,
    0x209188240001000ULL,
    0x400408a884001800ULL,
    0x110400a6080400ULL,
    0x1840060a44020800ULL,
    0x90080104000041ULL,
    0x201011000808101ULL,
    0x1a2208080504f080ULL,
    0x8012020600211212ULL,
    0x500861011240000ULL,
    0x180806108200800ULL,
    0x4000020e01040044ULL,
    0x300000261044000aULL,
    0x802241102020002ULL,
    0x20906061210001ULL,
    0x5a84841004010310ULL,
    0x4010801011c04ULL,
    0xa010109502200ULL,
    0x4a02012000ULL,
    0x500201010098b028ULL,
    0x8040002811040900ULL,
    0x28000010020204ULL,
    0x6000020202d0240ULL,
    0x8918844842082200ULL,
    0x4010011029020020ULL,
};

inline int rook_magic_index(int square, U64 occupancy){
    occupancy &= rook_masks[square];

    occupancy *= rook_magics[square];

    occupancy >>=(64-rook_relevant_bits[square]);

    return (int)occupancy;
}

inline int bishop_magic_index(int square, U64 occupancy){
    occupancy &= bishop_masks[square];

    occupancy *= bishop_magics[square];

    occupancy >>= (64-bishop_relevant_bits[square]);

    return (int)occupancy;
}


void init_rook_magic_table(){
    for(int square=0;square<64;square++){
        U64 mask = rook_masks[square];
        int bits = rook_relevant_bits[square];

        int occupancies = 1<<bits;

        for(int index=0;index<occupancies;index++){
            U64 occupancy = set_occupancy(index,bits,mask);

            int magic_index = rook_magic_index(square,occupancy);

            rook_attacks[square][magic_index] = rook_attacks_otf(square,occupancy);
        }
    }
}


void init_bishop_magic_table(){
    for(int square=0;square<64;square++){
        U64 mask = bishop_masks[square];

        int bits = bishop_relevant_bits[square];

        int occupancies = 1<<bits;

        for(int index = 0;index<occupancies;index++){
            U64 occupancy = set_occupancy(index,bits,mask);

            int magic_index = bishop_magic_index(square,occupancy);

            bishop_attacks[square][magic_index] = bishop_attacks_otf(square,occupancy);
        }
    }
}


inline U64 get_rook_attacks(
    int square,
    U64 occupancy)
{
    occupancy &= rook_masks[square];

    occupancy *= rook_magics[square];

    occupancy >>= (
        64 - rook_relevant_bits[square]
    );

    return rook_attacks[square][occupancy];
}

inline U64 get_bishop_attacks(
    int square,
    U64 occupancy)
{
    occupancy &= bishop_masks[square];

    occupancy *= bishop_magics[square];

    occupancy >>= (
        64 - bishop_relevant_bits[square]
    );

    return bishop_attacks[square][occupancy];
}

struct MoveList{
    uint32_t moves[256];
    int count;
};

inline void add_move(MoveList& list, uint32_t move){
    list.moves[list.count++]=move;
}

bool is_square_attacked(int square, int attackerColor, const Board& board);

void generate_knight_moves(MoveList& list,Board& board, int color){
    U64 knights = board.pieces[color][N];
    while(knights){
        int from = pop_lsb(knights);

        U64 attacks = knight_attacks[from];

        attacks &= (~board.occupancies[color]);

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,N,0,capture,0,0,0));
        }
    }
}

void generate_bishop_moves(MoveList& list, Board& board, int color){
    U64 bishops = board.pieces[color][B];

    while(bishops){
        int from = pop_lsb(bishops);
        U64 attacks = get_bishop_attacks(from,board.occupancies[BOTH]);

        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,B,0,capture,0,0,0));
        }
    }
}

void generate_rook_moves(MoveList& list, Board &board, int color){
    U64 rooks = board.pieces[color][R];

    while(rooks){
        int from = pop_lsb(rooks);
        U64 attacks = get_rook_attacks(from,board.occupancies[BOTH]);
        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,R,0,capture,0,0,0));
        }
    }
}

void generate_queen_moves(MoveList& list, Board& board, int color){
    U64 queens = board.pieces[color][Q];

    while(queens){
        int from = pop_lsb(queens);

        U64 attacks = get_rook_attacks(from,board.occupancies[BOTH])|get_bishop_attacks(from,board.occupancies[BOTH]);
        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);
            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,Q,0,capture,0,0,0));
        }
    }
}

void generate_pawn_moves(MoveList &list, Board &board, int color){
    U64 pawns = board.pieces[color][P];
    U64 enemy = board.occupancies[color^1];
    U64 empty = ~board.occupancies[BOTH];

    auto handlePromotion = [&](int from,int to,int capture){
        add_move(list,encode_move(from,to,P,Q,capture,0,0,0));
        add_move(list,encode_move(from,to,P,R,capture,0,0,0));
        add_move(list,encode_move(from,to,P,B,capture,0,0,0));
        add_move(list,encode_move(from,to,P,N,capture,0,0,0));
    };
    int from;
    int middle_to;
    int capture;
    int to;

    // En passant
    if(board.enPassant!=NO_SQ){
        to = board.enPassant;
        if(color==WHITE){
            from = to+7;
            if(from%8!=7 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }

            from = to+9;
            if(from%8!=0 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }
        }
        else{
            from = to-7;
            if(from%8!=0 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }

            from = to-9;
            if(from%8!=7 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }
        }
    }

    while(pawns){
        from = pop_lsb(pawns);

        // single pushes 
        capture = 0;
        if(color==WHITE) to = from-8;
        else to = from+8;
        // if the square moving to is empty, then its a pseudo-legal move
        if((1ULL<<to)&empty){
            if(color==WHITE&&to<8) handlePromotion(from,to,capture);
            else if(color==BLACK&&to>=56) handlePromotion(from,to,capture);
            else add_move(list,encode_move(from,to,P,0,0,0,0,0));
        }

        //double push, has to have started from original pawn square
        if(color==WHITE&&(from>=48&&from<=55)) {
            to = from-16;
            middle_to = from-8;
            if(((1ULL<<to)&empty)>0 && ((1ULL<<middle_to)&empty)>0){
                add_move(list,encode_move(from,to,P,0,0,1,0,0));
            }
        }
        else if(color==BLACK&&(from>=8&&from<=15))
        {
            to = from+16;
            middle_to = from+8;
            if(((1ULL<<to)&empty)>0 && ((1ULL<<middle_to)&empty)>0){
                add_move(list,encode_move(from,to,P,0,0,1,0,0));
            }
        } 

        // captures go diagonal
        if(color==WHITE){
            // goes -7 (capture right) and -9 (capture left)
            to = from-7;
            if(from%8 !=7 && (1ULL<<to)&enemy){
                if(to<8) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }

            to = from-9;
            if(from%8!=0 && (1ULL<<to)&enemy){
                if(to<8) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }
        }
        else{
            to = from+7;
            if(from%8 !=0 && (1ULL<<to)&enemy){
                if(to>=56) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }

            to = from+9;
            if(from%8!=7 && (1ULL<<to)&enemy){
                if(to>=56) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }
        }

    }
}

void generate_king_moves(MoveList &list, Board &board, int color){
    U64 kings = board.pieces[color][K];
    while(kings){
        int from = pop_lsb(kings);

        // normal attacks (one square)

        U64 attacks = king_attacks[from] & ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);
            bool capture = (board.occupancies[color^1]&(1ULL<<to))!=0;
            add_move(list,encode_move(from,to,K,0,capture,0,0,0));
        }

        // castling: squares between must be empty, and king may not start in,
        // pass through, or land on an attacked square
        if(color==WHITE){
            if (from == e1){
                if((board.castlingRights & WKCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<f1)|(1ULL<<g1))) &&
                   !is_square_attacked(e1,BLACK,board) &&
                   !is_square_attacked(f1,BLACK,board) &&
                   !is_square_attacked(g1,BLACK,board)){
                    add_move(list,encode_move(e1,g1,K,0,0,0,0,1));
                }

                if((board.castlingRights & WQCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<d1)|(1ULL<<c1)|(1ULL<<b1))) &&
                   !is_square_attacked(e1,BLACK,board) &&
                   !is_square_attacked(d1,BLACK,board) &&
                   !is_square_attacked(c1,BLACK,board)){
                    add_move(list,encode_move(e1,c1,K,0,0,0,0,1));
                }
            }
        }
        else{
            if(from==e8){
                if((board.castlingRights & BKCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<f8)|(1ULL<<g8))) &&
                   !is_square_attacked(e8,WHITE,board) &&
                   !is_square_attacked(f8,WHITE,board) &&
                   !is_square_attacked(g8,WHITE,board)){
                    add_move(list,encode_move(e8,g8,K,0,0,0,0,1));
                }

                if((board.castlingRights & BQCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<d8)|(1ULL<<c8)|(1ULL<<b8))) &&
                   !is_square_attacked(e8,WHITE,board) &&
                   !is_square_attacked(d8,WHITE,board) &&
                   !is_square_attacked(c8,WHITE,board)){
                    add_move(list,encode_move(e8,c8,K,0,0,0,0,1));
                }
            }
        }
    }
}


bool is_square_attacked(int square, int attackerColor, const Board& board) {
    if (attackerColor == WHITE) {
        if (pawn_attacks[BLACK][square] & board.pieces[WHITE][P]) return true;
        if (knight_attacks[square] & board.pieces[WHITE][N]) return true;
        if (king_attacks[square] & board.pieces[WHITE][K]) return true;
        if (get_bishop_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[WHITE][B] | board.pieces[WHITE][Q])) return true;
        if (get_rook_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[WHITE][R] | board.pieces[WHITE][Q])) return true;
    } else {
        if (pawn_attacks[WHITE][square] & board.pieces[BLACK][P]) return true;
        if (knight_attacks[square] & board.pieces[BLACK][N]) return true;
        if (king_attacks[square] & board.pieces[BLACK][K]) return true;
        if (get_bishop_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[BLACK][B] | board.pieces[BLACK][Q])) return true;
        if (get_rook_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[BLACK][R] | board.pieces[BLACK][Q])) return true;
    }

    return false;
}

void generate_moves(MoveList &list, Board &board, int color){
    list.count = 0;
    generate_pawn_moves(list,board,color);
    generate_knight_moves(list,board,color);
    generate_bishop_moves(list,board,color);
    generate_rook_moves(list,board,color);
    generate_queen_moves(list,board,color);
    generate_king_moves(list,board,color);
}

MoveList generate_legal_moves(Board &board){
    MoveList legal; legal.count = 0;
    MoveList pseudo; pseudo.count = 0;

    generate_moves(pseudo,board,board.sideToMove);

    for(int i=0;i<pseudo.count;i++){
        uint32_t move = pseudo.moves[i];

        Undo undo = make_move(board,move);

        int king_sq = get_lsb(board.pieces[board.sideToMove^1][K]);
        if(!is_square_attacked(king_sq,board.sideToMove,board)){
            legal.moves[legal.count++] = move;
        }
        unmake_move(board,move,undo);
    }

    return legal;
}

// like generate_legal_moves, but only captures and promotions survive the
// pseudo-legal filter, before the (relatively expensive) make/unmake
// legality check runs — used by quiescence search
MoveList generate_legal_captures(Board &board){
    MoveList legal; legal.count = 0;
    MoveList pseudo; pseudo.count = 0;

    generate_moves(pseudo,board,board.sideToMove);

    for(int i=0;i<pseudo.count;i++){
        uint32_t move = pseudo.moves[i];

        bool capture = (move>>20)&1;
        int promoted = (move>>16)&0xf;
        if(!capture && !promoted) continue;

        Undo undo = make_move(board,move);

        int king_sq = get_lsb(board.pieces[board.sideToMove^1][K]);
        if(!is_square_attacked(king_sq,board.sideToMove,board)){
            legal.moves[legal.count++] = move;
        }
        unmake_move(board,move,undo);
    }

    return legal;
}

const char* piece_name(int piece) {
    switch (piece) {
        case P: return "P";
        case N: return "N";
        case B: return "B";
        case R: return "R";
        case Q: return "Q";
        case K: return "K";
        default: return "?";
    }
}

const char* color_name(int color) {
    return color == WHITE ? "WHITE" : "BLACK";
}

void print_square(int square, char out[3]) {
    out[0] = 'a' + (square % 8);
    out[1] = '8' - (square / 8);
    out[2] = '\0';
}

void print_move(uint32_t move) {
    int from = FROM(move);
    int to = TO(move);
    int promoted = (move >> 16) & 0xf;
    bool capture = (move >> 20) & 1;
    bool double_push = (move >> 21) & 1;
    bool enpassant = (move >> 22) & 1;
    bool castling = (move >> 23) & 1;

    char from_sq[3];
    char to_sq[3];
    print_square(from, from_sq);
    print_square(to, to_sq);

    std::cout << from_sq << "->" << to_sq;
    if (promoted) std::cout << "=" << piece_name(promoted);
    if (capture) std::cout << "x";
    if (double_push) std::cout << "d";
    if (enpassant) std::cout << "e";
    if (castling) std::cout << "c";
}

void print_move_list(const MoveList& list) {
    for (int i = 0; i < list.count; i++) {
        print_move(list.moves[i]);
        if (i + 1 < list.count) std::cout << ", ";
    }
    std::cout << "\n";
}

void parse_fen(Board &board, const std::string &fen){
    memset(&board, 0, sizeof(Board));

    std::istringstream ss(fen);
    std::string boardPart, sidePart, castlePart, epPart;
    int halfmove = 0, fullmove = 1;

    ss >> boardPart >> sidePart >> castlePart >> epPart;
    if(!(ss >> halfmove)) halfmove = 0;
    if(!(ss >> fullmove)) fullmove = 1;

    int square = 0;
    for(char c : boardPart){
        if(c == '/') continue;
        if(isdigit((unsigned char)c)){
            square += c - '0';
            continue;
        }

        int color = isupper((unsigned char)c) ? WHITE : BLACK;
        int piece;
        switch(tolower(c)){
            case 'p': piece = P; break;
            case 'n': piece = N; break;
            case 'b': piece = B; break;
            case 'r': piece = R; break;
            case 'q': piece = Q; break;
            case 'k': piece = K; break;
            default: piece = -1; break;
        }
        assert(piece != -1);

        set_bit(board.pieces[color][piece], square);
        square++;
    }

    for(int c = WHITE; c <= BLACK; c++){
        for(int p = P; p <= K; p++){
            board.occupancies[c] |= board.pieces[c][p];
        }
    }
    board.occupancies[BOTH] = board.occupancies[WHITE] | board.occupancies[BLACK];

    board.sideToMove = (sidePart == "w") ? WHITE : BLACK;

    board.castlingRights = 0;
    if(castlePart.find('K') != std::string::npos) board.castlingRights |= WKCA;
    if(castlePart.find('Q') != std::string::npos) board.castlingRights |= WQCA;
    if(castlePart.find('k') != std::string::npos) board.castlingRights |= BKCA;
    if(castlePart.find('q') != std::string::npos) board.castlingRights |= BQCA;

    if(epPart == "-"){
        board.enPassant = NO_SQ;
    }
    else{
        int file = epPart[0] - 'a';
        int rank = epPart[1] - '0'; // 1..8, matching FEN rank digits
        board.enPassant = Square((8 - rank) * 8 + file);
    }

    board.halfmoveClock = halfmove;
    board.fullmoveNumber = fullmove;

    board.zobristKey = compute_zobrist_key(board);
}

uint64_t perft(Board &board, int depth){
    if(depth == 0) return 1ULL;

    MoveList moves = generate_legal_moves(board);
    uint64_t nodes = 0;

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        nodes += perft(board, depth - 1);
        unmake_move(board, moves.moves[i], undo);
    }

    return nodes;
}

// per-move node counts at the root, for tracking down a perft mismatch
// against a move that generates the wrong subtree
void perft_divide(Board &board, int depth){
    MoveList moves = generate_legal_moves(board);
    uint64_t total = 0;

    for(int i = 0; i < moves.count; i++){
        uint32_t move = moves.moves[i];
        Undo undo = make_move(board, move);
        uint64_t nodes = perft(board, depth - 1);
        unmake_move(board, move, undo);

        print_move(move);
        std::cout << ": " << nodes << "\n";
        total += nodes;
    }

    std::cout << "Total: " << total << "\n";
}

void run_perft_test(const std::string &fen, int depth, uint64_t expected){
    Board board;
    parse_fen(board, fen);

    uint64_t nodes = perft(board, depth);
    std::cout << "perft(" << depth << ") = " << nodes
               << "  expected " << expected
               << (nodes == expected ? "  OK" : "  MISMATCH") << "\n";
}

// walks the same tree as perft, but at every node checks the incrementally
// maintained zobristKey against a from-scratch recomputation; returns the
// number of nodes visited (mismatches are reported as they're found)
uint64_t verify_zobrist(Board &board, int depth, int &mismatches){
    if(board.zobristKey != compute_zobrist_key(board)){
        mismatches++;
        std::cout << "zobrist MISMATCH at depth " << depth << "\n";
    }

    if(depth == 0) return 1ULL;

    MoveList moves = generate_legal_moves(board);
    uint64_t nodes = 1ULL;

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        nodes += verify_zobrist(board, depth - 1, mismatches);
        unmake_move(board, moves.moves[i], undo);
    }

    return nodes;
}

void run_zobrist_test(const std::string &fen, int depth){
    Board board;
    parse_fen(board, fen);

    int mismatches = 0;
    uint64_t nodes = verify_zobrist(board, depth, mismatches);
    std::cout << "zobrist check to depth " << depth << ": " << nodes
               << " nodes, " << mismatches << " mismatches"
               << (mismatches == 0 ? "  OK" : "  FAILED") << "\n";
}

// ---------------------------------------------------------------------
// Evaluation: material + piece-square tables, based on Tomasz
// Michniewski's "Simplified Evaluation Function"
// (https://www.chessprogramming.org/Simplified_Evaluation_Function).
// Tables are written rank8-to-rank1, top-to-bottom, which is exactly our
// square numbering (a8=0 ... h1=63), so they're used as-is for WHITE;
// BLACK looks up the same table with the square mirrored vertically
// (sq ^ 56 flips rank_index while leaving the file untouched, since
// square = rank_index*8+file and 56 = 0b111000).
//
// The king table is phase-tapered (PeSTO-style, MAX_PHASE=24 based on
// remaining non-pawn material) between a middlegame table that rewards
// castled safety and an endgame table that rewards a centralized,
// active king — the one piece whose ideal placement flips hardest
// between game phases.
// ---------------------------------------------------------------------

const int material_value[6] = {100, 320, 330, 500, 900, 0};

const int pawn_pst[64] = {
     0,   0,   0,   0,   0,   0,  0,   0,
    50,  50,  50,  50,  50,  50, 50,  50,
    10,  10,  20,  30,  30,  20, 10,  10,
     5,   5,  10,  25,  25,  10,  5,   5,
     0,   0,   0,  20,  20,   0,  0,   0,
     5,  -5, -10,   0,   0, -10, -5,   5,
     5,  10,  10, -20, -20,  10, 10,   5,
     0,   0,   0,   0,   0,   0,  0,   0,
};

const int knight_pst[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

const int bishop_pst[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

const int rook_pst[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     5,  10,  10,  10,  10,  10,  10,   5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
     0,   0,   0,   5,   5,   0,   0,   0,
};

const int queen_pst[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};

const int king_mg_pst[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20,
};

const int king_eg_pst[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50,
};

const int piece_phase_weight[6] = {0, 1, 1, 2, 4, 0}; // P,N,B,R,Q,K
const int MAX_PHASE = 24; // 2 * (2*N + 2*B + 2*R*2 + Q*4) per side

inline int pst_value(int piece, int sq){
    switch(piece){
        case P: return pawn_pst[sq];
        case N: return knight_pst[sq];
        case B: return bishop_pst[sq];
        case R: return rook_pst[sq];
        case Q: return queen_pst[sq];
        default: return 0;
    }
}

int compute_phase(const Board &board){
    int phase = 0;
    for(int c = WHITE; c <= BLACK; c++){
        for(int p = N; p <= Q; p++){
            phase += piece_phase_weight[p] * __builtin_popcountll(board.pieces[c][p]);
        }
    }
    return std::min(phase, MAX_PHASE);
}

// positive = good for White, regardless of side to move
int evaluate(const Board &board){
    int phase = compute_phase(board);
    int score = 0;

    for(int c = WHITE; c <= BLACK; c++){
        int sign = (c == WHITE) ? 1 : -1;

        for(int p = P; p <= K; p++){
            U64 bb = board.pieces[c][p];
            while(bb){
                int sq = pop_lsb(bb);
                int pstSq = (c == WHITE) ? sq : (sq ^ 56);

                int value = material_value[p];
                if(p == K){
                    value += (king_mg_pst[pstSq] * phase + king_eg_pst[pstSq] * (MAX_PHASE - phase)) / MAX_PHASE;
                } else {
                    value += pst_value(p, pstSq);
                }

                score += sign * value;
            }
        }
    }

    return score;
}

// ---------------------------------------------------------------------
// Search: negamax with alpha-beta pruning and MVV-LVA move ordering
// ---------------------------------------------------------------------

const int MATE_VALUE = 32000;
const int INF_SCORE = 1000000;
const int MATE_THRESHOLD = MATE_VALUE - 512; // anything beyond this margin is a mate score, not a real eval

// ---------------------------------------------------------------------
// Time management. There's no search thread here — negamax/quiescence
// just poll a deadline periodically (checking a clock on every single
// node would itself be a meaningful slowdown, hence the node-count mask)
// and set a sticky "stop" flag once time is up. Every node still in
// flight when that happens returns immediately with a throwaway value;
// the only thing that matters is that the root driver (iterative_deepening)
// discards that entire in-progress iteration rather than trusting it,
// and that negamax skips its TT store whenever g_stop is set, so no
// corrupted score from an aborted search ever gets persisted.
// ---------------------------------------------------------------------

bool g_stop = false;
uint64_t g_nodes = 0;
bool g_time_limited = false;
std::chrono::steady_clock::time_point g_deadline;

inline bool check_time(){
    g_nodes++;
    if(g_stop) return true;
    if(g_time_limited && (g_nodes & 2047) == 0){
        if(std::chrono::steady_clock::now() >= g_deadline) g_stop = true;
    }
    return g_stop;
}

// ---------------------------------------------------------------------
// Killer moves and the history heuristic — quiet-move ordering signals
// that complement the TT best-move hint and MVV-LVA (see explanations.txt
// for the reasoning behind both of these).
// ---------------------------------------------------------------------

const int MAX_PLY = 128;
uint32_t killer_moves[MAX_PLY][2];
int history_table[2][64][64];

void clear_search_heuristics(){
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_table, 0, sizeof(history_table));
}

// halves rather than clears: keeps cross-search knowledge without letting
// scores from an earlier, easier position dominate move ordering forever
void age_history(){
    for(int c = 0; c < 2; c++)
        for(int f = 0; f < 64; f++)
            for(int t = 0; t < 64; t++)
                history_table[c][f][t] /= 2;
}

// ---------------------------------------------------------------------
// Transposition table, keyed on the Zobrist hash. Stores a fail-soft
// score bound (exact/lower/upper), the depth it was searched to, and the
// best move found — the latter matters even when the depth is too
// shallow for a cutoff, since it's still the strongest move-ordering hint
// available (searched first, so alpha-beta narrows the window fastest).
// ---------------------------------------------------------------------

enum TTFlag { TT_EXACT, TT_LOWERBOUND, TT_UPPERBOUND };

struct TTEntry{
    U64 key;
    int depth; // -1 marks an empty slot
    int score;
    uint32_t bestMove;
    TTFlag flag;
};

const size_t TT_SIZE = 1ULL << 20; // ~1M entries (~24MB)
TTEntry tt_table[TT_SIZE];

void tt_clear(){
    for(size_t i = 0; i < TT_SIZE; i++) tt_table[i].depth = -1;
}

TTEntry* tt_probe(U64 key){
    TTEntry &e = tt_table[key & (TT_SIZE - 1)];
    if(e.depth >= 0 && e.key == key) return &e;
    return nullptr;
}

// depth-preferred replacement: never let a shallow re-search evict a
// deeper, more valuable entry for the same key
void tt_store(U64 key, int depth, int score, TTFlag flag, uint32_t bestMove){
    TTEntry &e = tt_table[key & (TT_SIZE - 1)];
    if(e.depth < 0 || e.key != key || depth >= e.depth){
        e.key = key;
        e.depth = depth;
        e.score = score;
        e.flag = flag;
        e.bestMove = bestMove;
    }
}

// Mate scores encode distance-to-mate from the CURRENT node ("mate in N"),
// but that distance means something different at every ply from the root.
// Storing/retrieving through the TT re-bases the score onto the node
// where it's being stored or read, so a transposed path to the same
// subtree still reports the correct mate distance from wherever it sits.
int value_to_tt(int score, int ply){
    if(score >= MATE_THRESHOLD) return score + ply;
    if(score <= -MATE_THRESHOLD) return score - ply;
    return score;
}

int value_from_tt(int score, int ply){
    if(score >= MATE_THRESHOLD) return score - ply;
    if(score <= -MATE_THRESHOLD) return score + ply;
    return score;
}

// score used only to order moves before searching them; higher = search first
int mvv_lva_score(const Board &board, uint32_t move){
    bool capture = (move >> 20) & 1;
    if(!capture) return -1;

    bool enpassant = (move >> 22) & 1;
    int attacker = PIECE(move);

    int victim = P;
    if(!enpassant){
        int to = TO(move);
        int them = board.sideToMove ^ 1;
        for(int pt = P; pt <= K; pt++){
            if(board.pieces[them][pt] & (1ULL << to)){
                victim = pt;
                break;
            }
        }
    }

    static const int piece_value[6] = {100, 320, 330, 500, 900, 20000};
    return piece_value[victim] * 10 - piece_value[attacker];
}

void order_moves(const Board &board, MoveList &moves, uint32_t tt_move = 0){
    std::sort(moves.moves, moves.moves + moves.count, [&](uint32_t a, uint32_t b){
        int sa = (tt_move && a == tt_move) ? INF_SCORE : mvv_lva_score(board, a);
        int sb = (tt_move && b == tt_move) ? INF_SCORE : mvv_lva_score(board, b);
        return sa > sb;
    });
}

// Full ordering priority, highest to lowest: TT move, then captures by
// MVV-LVA, then killer moves for this ply, then quiet moves by history
// score. Used only by the main search (negamax and the root) — quiescence
// only ever considers captures/evasions, so it has nothing for killers or
// history to distinguish and just uses the plain order_moves above.
int move_order_score(const Board &board, uint32_t move, int ply, uint32_t tt_move){
    if(tt_move && move == tt_move) return 2000000;

    bool capture = (move >> 20) & 1;
    if(capture) return 1000000 + mvv_lva_score(board, move);

    int killer_ply = std::min(ply, MAX_PLY - 1); // deep explicit-depth searches can exceed MAX_PLY
    if(move == killer_moves[killer_ply][0]) return 900000;
    if(move == killer_moves[killer_ply][1]) return 800000;

    return std::min(history_table[board.sideToMove][FROM(move)][TO(move)], 500000);
}

void order_moves_with_heuristics(const Board &board, MoveList &moves, int ply, uint32_t tt_move = 0){
    std::sort(moves.moves, moves.moves + moves.count, [&](uint32_t a, uint32_t b){
        return move_order_score(board, a, ply, tt_move) > move_order_score(board, b, ply, tt_move);
    });
}

// Quiescence search: resolves tactical sequences (captures, promotions,
// and — if the side to move is in check — every legal reply) past the
// main search's depth cutoff, so the static eval is never sampled in the
// middle of a capture exchange. "Stand pat" reflects that a side is never
// forced to capture: if staying quiet already beats beta, or is the best
// option found so far, that's a valid alpha-beta bound in its own right.
// If in check there's no stand-pat option — sitting still isn't legal —
// so every legal move must be considered, mirroring negamax's mate logic.
int quiescence(Board &board, int ply, int alpha, int beta){
    if(check_time()) return 0;

    int king_sq = get_lsb(board.pieces[board.sideToMove][K]);
    bool in_check = is_square_attacked(king_sq, board.sideToMove ^ 1, board);

    if(!in_check){
        int e = evaluate(board);
        int stand_pat = (board.sideToMove == WHITE) ? e : -e;

        if(stand_pat >= beta) return beta;
        if(stand_pat > alpha) alpha = stand_pat;
    }

    MoveList moves = in_check ? generate_legal_moves(board) : generate_legal_captures(board);

    if(in_check && moves.count == 0) return -(MATE_VALUE - ply);

    order_moves(board, moves);

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        int score = -quiescence(board, ply + 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

// returns the score from the perspective of the side to move at this node
int negamax(Board &board, int depth, int ply, int alpha, int beta){
    if(check_time()) return 0;

    if(depth == 0){
        return quiescence(board, ply, alpha, beta);
    }

    int alpha_orig = alpha;
    uint32_t tt_move = 0;

    TTEntry *entry = tt_probe(board.zobristKey);
    if(entry){
        tt_move = entry->bestMove;
        if(entry->depth >= depth){
            int tt_score = value_from_tt(entry->score, ply);
            if(entry->flag == TT_EXACT) return tt_score;
            if(entry->flag == TT_UPPERBOUND && tt_score <= alpha) return tt_score;
            if(entry->flag == TT_LOWERBOUND && tt_score >= beta) return tt_score;
        }
    }

    MoveList moves = generate_legal_moves(board);
    if(moves.count == 0){
        int king_sq = get_lsb(board.pieces[board.sideToMove][K]);
        bool in_check = is_square_attacked(king_sq, board.sideToMove ^ 1, board);
        if(in_check) return -(MATE_VALUE - ply); // checkmated; prefer surviving longer
        return 0; // stalemate
    }

    order_moves_with_heuristics(board, moves, ply, tt_move);

    int best = -INF_SCORE;
    uint32_t best_move = moves.moves[0];
    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        int score = -negamax(board, depth - 1, ply + 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score > best){
            best = score;
            best_move = moves.moves[i];
        }
        if(best > alpha) alpha = best;
        if(alpha >= beta){
            // a quiet move refuting this many siblings is worth remembering,
            // both for sibling nodes at this same ply (killers) and for
            // quiet moves sharing this from/to anywhere in the tree (history)
            bool capture = (moves.moves[i] >> 20) & 1;
            if(!capture){
                int killer_ply = std::min(ply, MAX_PLY - 1);
                if(moves.moves[i] != killer_moves[killer_ply][0]){
                    killer_moves[killer_ply][1] = killer_moves[killer_ply][0];
                    killer_moves[killer_ply][0] = moves.moves[i];
                }
                history_table[board.sideToMove][FROM(moves.moves[i])][TO(moves.moves[i])] += depth * depth;
            }
            break; // beta cutoff
        }
    }

    // if time ran out partway through this node's move loop, `best` may
    // reflect moves cut short by check_time()'s throwaway 0 — never let
    // that leak into the table
    if(!g_stop){
        TTFlag flag = (best <= alpha_orig) ? TT_UPPERBOUND : (best >= beta) ? TT_LOWERBOUND : TT_EXACT;
        tt_store(board.zobristKey, depth, value_to_tt(best, ply), flag, best_move);
    }

    return best;
}

int square_from_uci(const std::string &s){
    int file = s[0] - 'a';
    int rank = s[1] - '0'; // 1..8
    return (8 - rank) * 8 + file;
}

std::string uci_from_square(int sq){
    std::string s;
    s += char('a' + sq % 8);
    s += char('0' + (8 - sq / 8));
    return s;
}

std::string move_to_uci(uint32_t move){
    std::string s = uci_from_square(FROM(move)) + uci_from_square(TO(move));
    switch((move >> 16) & 0xf){
        case Q: s += 'q'; break;
        case R: s += 'r'; break;
        case B: s += 'b'; break;
        case N: s += 'n'; break;
        default: break;
    }
    return s;
}

uint32_t find_best_move(Board &board, int depth, int &out_score){
    MoveList moves = generate_legal_moves(board);

    TTEntry *entry = tt_probe(board.zobristKey);
    order_moves_with_heuristics(board, moves, 0, entry ? entry->bestMove : 0);

    int alpha = -INF_SCORE, beta = INF_SCORE;
    int best_score = -INF_SCORE;
    uint32_t best_move = 0;

    for(int i = 0; i < moves.count; i++){
        if(g_stop) break; // time's up mid-iteration; caller discards this depth's result anyway

        Undo undo = make_move(board, moves.moves[i]);
        int score = -negamax(board, depth - 1, 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score > best_score){
            best_score = score;
            best_move = moves.moves[i];
        }
        if(best_score > alpha) alpha = best_score;
    }

    out_score = best_score;
    return best_move;
}

struct SearchInfo{
    uint32_t bestMove = 0;
    int score = 0;
    int depthReached = 0;
    uint64_t nodes = 0;
};

// Iterative deepening: searches depth 1, 2, 3, ... in sequence, re-using
// the TT filled in by each shallower pass to order moves in the next —
// which is what makes searching all those "redundant" shallow depths
// nearly free rather than wasted work (see the depth-1-through-8-vs-cold-
// depth-8 timing comparison from the previous session). It also gives us
// a well-defined way to stop on a clock: only a FULLY completed depth is
// ever trusted. If the clock runs out mid-iteration, that iteration's
// result is thrown away and the previous depth's move is kept — except
// depth 1, which is always accepted so the engine never has "no move" to
// return, even under absurd time pressure.
SearchInfo iterative_deepening(Board &board, int maxDepth, long long timeLimitMs, bool report){
    g_stop = false;
    g_nodes = 0;
    g_time_limited = (timeLimitMs >= 0);

    // killers are ply-indexed guesses shaped by THIS tree, so a previous
    // search's killers are irrelevant here; history generalizes better
    // across searches, so it's aged rather than wiped (see explanations.txt)
    memset(killer_moves, 0, sizeof(killer_moves));
    age_history();

    auto start = std::chrono::steady_clock::now();
    if(g_time_limited) g_deadline = start + std::chrono::milliseconds(timeLimitMs);

    SearchInfo result;

    for(int depth = 1; depth <= maxDepth; depth++){
        if(g_stop) break;

        int score;
        uint32_t move = find_best_move(board, depth, score);
        bool aborted = g_stop;

        if(aborted && depth > 1) break; // partial result at this depth is unreliable; keep the last complete one

        result.bestMove = move;
        result.score = score;
        result.depthReached = depth;
        result.nodes = g_nodes;

        if(report){
            long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            std::cout << "info depth " << depth << " score cp " << score
                       << " nodes " << g_nodes << " time " << elapsed
                       << " pv " << move_to_uci(move) << "\n" << std::flush;
        }

        if(aborted) break;
        if(score >= MATE_THRESHOLD || score <= -MATE_THRESHOLD) break; // forced mate found; deeper can't do better
    }

    return result;
}

// ---------------------------------------------------------------------
// UCI protocol
// ---------------------------------------------------------------------

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const int DEFAULT_SEARCH_DEPTH = 6; // fallback for a bare "go" with no depth/time info at all

// disambiguates castling/en-passant/promotion by matching against the
// actual legal move list rather than re-deriving the flags from the
// bare from/to squares UCI gives us
uint32_t find_move_by_uci(Board &board, const std::string &uci){
    int from = square_from_uci(uci.substr(0, 2));
    int to = square_from_uci(uci.substr(2, 2));

    int promo = 0;
    if(uci.size() >= 5){
        switch(tolower((unsigned char)uci[4])){
            case 'q': promo = Q; break;
            case 'r': promo = R; break;
            case 'b': promo = B; break;
            case 'n': promo = N; break;
        }
    }

    MoveList moves = generate_legal_moves(board);
    for(int i = 0; i < moves.count; i++){
        uint32_t mv = moves.moves[i];
        if((int)FROM(mv) == from && (int)TO(mv) == to && (int)((mv >> 16) & 0xf) == promo){
            return mv;
        }
    }
    return 0;
}

void uci_loop(){
    Board board;
    parse_fen(board, START_FEN);

    std::string line;
    while(std::getline(std::cin, line)){
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if(cmd == "uci"){
            std::cout << "id name ChessEngine\n";
            std::cout << "id author Jerry\n";
            std::cout << "uciok\n" << std::flush;
        }
        else if(cmd == "isready"){
            std::cout << "readyok\n" << std::flush;
        }
        else if(cmd == "ucinewgame"){
            tt_clear();
            clear_search_heuristics();
            parse_fen(board, START_FEN);
        }
        else if(cmd == "position"){
            std::string token;
            iss >> token;

            if(token == "startpos"){
                parse_fen(board, START_FEN);
            }
            else if(token == "fen"){
                std::string fen, part;
                for(int i = 0; i < 6 && (iss >> part); i++){
                    if(i) fen += " ";
                    fen += part;
                }
                parse_fen(board, fen);
            }

            std::string movesToken;
            if(iss >> movesToken && movesToken == "moves"){
                std::string moveStr;
                while(iss >> moveStr){
                    uint32_t mv = find_move_by_uci(board, moveStr);
                    if(mv) make_move(board, mv);
                }
            }
        }
        else if(cmd == "go"){
            bool explicitDepth = false;
            int depthArg = 0;
            long long movetime = -1, wtime = -1, btime = -1, winc = 0, binc = 0;

            std::string token;
            while(iss >> token){
                if(token == "depth"){ iss >> depthArg; explicitDepth = true; }
                else if(token == "movetime") iss >> movetime;
                else if(token == "wtime") iss >> wtime;
                else if(token == "btime") iss >> btime;
                else if(token == "winc") iss >> winc;
                else if(token == "binc") iss >> binc;
                // movestogo/ponder/infinite/searchmoves intentionally unhandled
            }

            int maxDepth;
            long long timeLimitMs = -1; // -1 = unlimited

            if(explicitDepth){
                // an explicit depth request searches exactly that depth via
                // iterative deepening, uninterrupted by the clock
                maxDepth = depthArg;
            }
            else if(movetime >= 0){
                maxDepth = 64;
                timeLimitMs = std::max(movetime - 50, 50LL); // 50ms safety margin for UCI I/O overhead
            }
            else if(wtime >= 0 || btime >= 0){
                maxDepth = 64;
                long long myTime = std::max((board.sideToMove == WHITE) ? wtime : btime, 0LL);
                long long myInc  = (board.sideToMove == WHITE) ? winc : binc;

                // simple fixed-fraction budget: assume ~20 moves left, never
                // risk more than half the clock, always leave enough for
                // at least a near-instant move
                long long budget = myTime / 20 + myInc;
                budget = std::min(budget, myTime / 2);
                budget = std::max(budget, 50LL);
                timeLimitMs = budget;
            }
            else{
                // bare "go" (or "go infinite", which we can't truly honor
                // without a search thread to interrupt): fall back to a
                // fixed depth
                maxDepth = DEFAULT_SEARCH_DEPTH;
            }

            SearchInfo info = iterative_deepening(board, maxDepth, timeLimitMs, true);
            std::cout << "bestmove " << (info.bestMove ? move_to_uci(info.bestMove) : "0000") << "\n" << std::flush;
        }
        else if(cmd == "stop"){
            // search is synchronous and already finished by the time "go"
            // returns, so there's nothing to interrupt
        }
        else if(cmd == "quit"){
            break;
        }
        // unrecognized commands are ignored, per the UCI convention
    }
}

int main(int argc, char** argv){
    init_knight_attacks();
    init_king_attacks();
    init_pawn_attacks();

    init_slider_masks();
    initialize_relevancy_bits();

    init_rook_magic_table();
    init_bishop_magic_table();

    init_zobrist();
    tt_clear();
    clear_search_heuristics();

    if(argc >= 3 && std::string(argv[1]) == "eval"){
        Board board;
        parse_fen(board, argv[2]);
        std::cout << evaluate(board) << "\n";
        return 0;
    }

    if(argc >= 4 && std::string(argv[1]) == "bestmove"){
        Board board;
        parse_fen(board, argv[2]);
        int depth = std::atoi(argv[3]);

        SearchInfo info = iterative_deepening(board, depth, -1, false);

        print_move(info.bestMove);
        std::cout << "  score: " << info.score << "  depth: " << info.depthReached
                   << "  nodes: " << info.nodes << "\n";
        return 0;
    }

    if(argc >= 3 && std::string(argv[1]) == "key"){
        Board board;
        parse_fen(board, argv[2]);
        std::cout << std::hex << board.zobristKey << std::dec << "\n";
        return 0;
    }

    if(argc >= 4 && (std::string(argv[1]) == "divide" || std::string(argv[1]) == "perft" || std::string(argv[1]) == "zobrist")){
        std::string mode = argv[1];
        std::string fen = argv[2];
        int depth = std::atoi(argv[3]);

        Board board;
        parse_fen(board, fen);

        if(mode == "divide") perft_divide(board, depth);
        else if(mode == "zobrist") run_zobrist_test(fen, depth);
        else std::cout << perft(board, depth) << "\n";

        return 0;
    }

    if(argc >= 2 && std::string(argv[1]) == "test"){
        const std::string kiwipete =
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

        std::cout << "startpos:\n";
        run_perft_test(START_FEN, 1, 20);
        run_perft_test(START_FEN, 2, 400);
        run_perft_test(START_FEN, 3, 8902);
        run_perft_test(START_FEN, 4, 197281);
        run_perft_test(START_FEN, 5, 4865609);
        run_zobrist_test(START_FEN, 5);

        std::cout << "\nkiwipete:\n";
        run_perft_test(kiwipete, 1, 48);
        run_perft_test(kiwipete, 2, 2039);
        run_perft_test(kiwipete, 3, 97862);
        run_perft_test(kiwipete, 4, 4085603);
        run_zobrist_test(kiwipete, 4);

        return 0;
    }

    // no recognized subcommand: behave like a normal UCI engine binary,
    // which is what GUIs expect when they launch it with no arguments
    uci_loop();
    return 0;
}




