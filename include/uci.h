#pragma once

#include "board.h"
#include <string>
#include <cstdint>

extern const std::string START_FEN;
extern const int DEFAULT_SEARCH_DEPTH; // fallback for a bare "go" with no depth/time info at all

// disambiguates castling/en-passant/promotion by matching against the
// actual legal move list rather than re-deriving the flags from the
// bare from/to squares UCI gives us
uint32_t find_move_by_uci(Board &board, const std::string &uci);

void uci_loop();
