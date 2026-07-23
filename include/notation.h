#pragma once

#include <cstdint>
#include <string>

// pure move<->UCI-string conversions; no board state needed (see uci.h's
// find_move_by_uci for the part that does need a board, to disambiguate
// castling/en-passant/promotion against the actual legal move list)
int square_from_uci(const std::string &s);
std::string uci_from_square(int sq);
std::string move_to_uci(uint32_t move);
