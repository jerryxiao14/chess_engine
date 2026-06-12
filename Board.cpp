#include <iostream>
#include <cstdint>
#include <bit>
#include <cassert>


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
    a1, b1, c1, d1, e1, f1, g1, h1,
};

inline void set_bit(U64 &bitboard,int square){
    bitboard |= (1ULL<<square);
}

inline int get_bit(U64 &bitboard,int square){
    return (bitboard & (1ULL<<square));
}

inline void pop_bit(U64 &bitboard, int square){
    bitboard &= ~(1ULL<<square);
}

inline int pop_lsb(U64 &bb){
    int square = __builtin_ctzll(bb);
    bb &= bb-1;
    return square;
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


// bitboard visualization

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

    attacks |= (pawn << 7) & NOT_H_FILE;
    attacks |= (pawn << 9) & NOT_A_FILE;

    return attacks;
}

U64 mask_black_pawn_attacks(int square)
{
    U64 attacks = 0ULL;

    U64 pawn = 1ULL << square;

    attacks |= (pawn >> 7) & NOT_A_FILE;
    attacks |= (pawn >> 9) & NOT_H_FILE;

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

int main(){
    init_knight_attacks();
    init_king_attacks();
    init_pawn_attacks();

    init_slider_masks();
    initialize_relevancy_bits();

    init_rook_magic_table();
    init_bishop_magic_table();


    for(int sq = 0;sq<64;sq++){
        U64 mask = rook_masks[sq];

        int bits = rook_relevant_bits[sq];

        int occupancies = 1<<bits;

        for(int idx = 0; idx<occupancies;idx++){
            U64 occ = set_occupancy(idx,bits,mask);
            //U64 slow = rook_attacks_otf(sq,occ);
            //U64 fast = get_rook_attacks(sq,occ);

            U64 slow = bishop_attacks_otf(sq,occ);
            U64  fast = get_bishop_attacks(sq,occ);

            if(slow != fast)
            {
                std::cout << "FAIL\n";
                std::cout << "square = " << sq << "\n";
                std::cout << "index = " << idx << "\n";

                std::cout << "occupancy = "
                        << std::hex << occ << "\n";

                std::cout << "slow = "
                        << std::hex << slow << "\n";

                std::cout << "fast = "
                        << std::hex << fast << "\n";

                print_bitboard(occ);
                print_bitboard(slow);
                print_bitboard(fast);

                return 0;
            }
        }
    }

    return 0;
}




