#pragma once

#include "types.h"
#include "movegen.h"
#include <cstdint>

void print_bitboard(U64 bitboard);

const char* piece_name(int piece);
const char* color_name(int color);

void print_square(int square, char out[3]);
void print_move(uint32_t move);
void print_move_list(const MoveList& list);
