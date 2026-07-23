#pragma once

#include "types.h"
#include <cstdint>

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

/*
Move encoding (packed into a plain uint32_t, no dedicated Move type):
bits 0-5:   from square
bits 6-11:  to square
bits 12-15: piece
bits 16-19: promoted piece
bit 20:     capture
bit 21:     double pawn push
bit 22:     en passant
bit 23:     castling
*/
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

#define FROM(move) ((move)&0x3f)
#define TO(move) ((move>>6)&0x3f)
#define PIECE(move) (((move)>>12) & 0xf)
