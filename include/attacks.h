#pragma once

#include "types.h"

// leaper attack tables (populated by the init_* functions below)
extern U64 knight_attacks[64];
extern U64 king_attacks[64];
extern U64 pawn_attacks[2][64];

void init_knight_attacks();
void init_king_attacks();
void init_pawn_attacks();

// magic-bitboard slider attacks
extern U64 rook_masks[64];
extern U64 bishop_masks[64];
extern int rook_relevant_bits[64];
extern int bishop_relevant_bits[64];
extern U64 rook_attacks[64][4096];
extern U64 bishop_attacks[64][512];
extern const U64 rook_magics[64];
extern const U64 bishop_magics[64];

void init_slider_masks();
void initialize_relevancy_bits();
void init_rook_magic_table();
void init_bishop_magic_table();

// hot path: kept inline so callers in movegen/search don't pay a call
// overhead on top of the table lookup
inline U64 get_rook_attacks(int square, U64 occupancy){
    occupancy &= rook_masks[square];
    occupancy *= rook_magics[square];
    occupancy >>= (64 - rook_relevant_bits[square]);
    return rook_attacks[square][occupancy];
}

inline U64 get_bishop_attacks(int square, U64 occupancy){
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magics[square];
    occupancy >>= (64 - bishop_relevant_bits[square]);
    return bishop_attacks[square][occupancy];
}
