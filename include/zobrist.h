#pragma once

#include "types.h"
#include "board.h"

extern U64 piece_keys[2][6][64];
extern U64 side_key;
extern U64 castle_right_key[4]; // one key per WKCA/WQCA/BKCA/BQCA bit
extern U64 castling_keys[16];   // XOR of castle_right_key[] for each combination of rights
extern U64 en_passant_keys[8];  // keyed by file, since only the file matters for ep availability

void init_zobrist();

inline U64 ep_zobrist(Square sq){
    return (sq == NO_SQ) ? 0ULL : en_passant_keys[sq % 8];
}

U64 compute_zobrist_key(const Board &board);
