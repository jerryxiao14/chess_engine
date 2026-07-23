#pragma once

#include "board.h"
#include <cstdint>

Undo make_move(Board &board, uint32_t move);
void unmake_move(Board &board, uint32_t move, const Undo &undo);
