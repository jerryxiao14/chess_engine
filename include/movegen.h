#pragma once

#include "board.h"
#include <cstdint>

struct MoveList{
    uint32_t moves[256];
    int count;
};

inline void add_move(MoveList& list, uint32_t move){
    list.moves[list.count++]=move;
}

bool is_square_attacked(int square, int attackerColor, const Board& board);

void generate_knight_moves(MoveList& list, Board& board, int color);
void generate_bishop_moves(MoveList& list, Board& board, int color);
void generate_rook_moves(MoveList& list, Board& board, int color);
void generate_queen_moves(MoveList& list, Board& board, int color);
void generate_pawn_moves(MoveList& list, Board& board, int color);
void generate_king_moves(MoveList& list, Board& board, int color);

void generate_moves(MoveList &list, Board &board, int color);

MoveList generate_legal_moves(Board &board);

// like generate_legal_moves, but only captures and promotions survive the
// pseudo-legal filter, before the (relatively expensive) make/unmake
// legality check runs — used by quiescence search
MoveList generate_legal_captures(Board &board);
