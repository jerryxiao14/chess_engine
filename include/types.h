#pragma once

#include <cstdint>

typedef uint64_t U64;

enum Color { WHITE, BLACK, BOTH };
enum Piece { P, N, B, R, Q, K };

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

inline void set_bit(U64 &bitboard, int square){
    bitboard |= (1ULL << square);
}

inline int get_bit(U64 &bitboard, int square){
    return (bitboard >> square) & 1ULL;
}

inline void pop_bit(U64 &bitboard, int square){
    bitboard &= ~(1ULL << square);
}

inline int pop_lsb(U64 &bb){
    int square = __builtin_ctzll(bb);
    bb &= bb - 1;
    return square;
}

inline int get_lsb(U64 bb){
    return __builtin_ctzll(bb);
}
