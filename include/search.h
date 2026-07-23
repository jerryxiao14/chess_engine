#pragma once

#include "board.h"
#include "movegen.h"
#include <cstdint>
#include <chrono>

// ---------------------------------------------------------------------
// Time management. There's no search thread here — negamax/quiescence
// just poll a deadline periodically (checking a clock on every single
// node would itself be a meaningful slowdown, hence the node-count mask)
// and set a sticky "stop" flag once time is up. Every node still in
// flight when that happens returns immediately with a throwaway value;
// the only thing that matters is that the root driver (iterative_deepening)
// discards that entire in-progress iteration rather than trusting it,
// and that negamax skips its TT store whenever g_stop is set, so no
// corrupted score from an aborted search ever gets persisted.
// ---------------------------------------------------------------------

extern bool g_stop;
extern uint64_t g_nodes;
extern bool g_time_limited;
extern std::chrono::steady_clock::time_point g_deadline;

bool check_time();

// ---------------------------------------------------------------------
// Killer moves and the history heuristic — quiet-move ordering signals
// that complement the TT best-move hint and MVV-LVA (see explanations.txt
// for the reasoning behind both of these).
// ---------------------------------------------------------------------

const int MAX_PLY = 128;
extern uint32_t killer_moves[MAX_PLY][2];
extern int history_table[2][64][64];

void clear_search_heuristics();
void age_history();

const int INF_SCORE = 1000000;

int mvv_lva_score(const Board &board, uint32_t move);
void order_moves(const Board &board, MoveList &moves, uint32_t tt_move = 0);

int move_order_score(const Board &board, uint32_t move, int ply, uint32_t tt_move);
void order_moves_with_heuristics(const Board &board, MoveList &moves, int ply, uint32_t tt_move = 0);

int quiescence(Board &board, int ply, int alpha, int beta);

// returns the score from the perspective of the side to move at this node
int negamax(Board &board, int depth, int ply, int alpha, int beta);

uint32_t find_best_move(Board &board, int depth, int &out_score);

struct SearchInfo{
    uint32_t bestMove = 0;
    int score = 0;
    int depthReached = 0;
    uint64_t nodes = 0;
};

// Iterative deepening: searches depth 1, 2, 3, ... in sequence, re-using
// the TT filled in by each shallower pass to order moves in the next —
// which is what makes searching all those "redundant" shallow depths
// nearly free rather than wasted work. It also gives us a well-defined
// way to stop on a clock: only a FULLY completed depth is ever trusted.
// If the clock runs out mid-iteration, that iteration's result is thrown
// away and the previous depth's move is kept — except depth 1, which is
// always accepted so the engine never has "no move" to return, even
// under absurd time pressure.
SearchInfo iterative_deepening(Board &board, int maxDepth, long long timeLimitMs, bool report);
