#pragma once

#include "board.h"
#include <cstdint>
#include <string>

uint64_t perft(Board &board, int depth);

// per-move node counts at the root, for tracking down a perft mismatch
// against a move that generates the wrong subtree
void perft_divide(Board &board, int depth);

void run_perft_test(const std::string &fen, int depth, uint64_t expected);

// walks the same tree as perft, but at every node checks the incrementally
// maintained zobristKey against a from-scratch recomputation; returns the
// number of nodes visited (mismatches are reported as they're found)
uint64_t verify_zobrist(Board &board, int depth, int &mismatches);

void run_zobrist_test(const std::string &fen, int depth);
