#include "zobrist.h"
#include <random>

U64 piece_keys[2][6][64];
U64 side_key;
U64 castle_right_key[4];
U64 castling_keys[16];
U64 en_passant_keys[8];

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
