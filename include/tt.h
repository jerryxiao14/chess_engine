#pragma once

#include "types.h"
#include <cstdint>
#include <cstddef>

const int MATE_VALUE = 32000;
const int MATE_THRESHOLD = MATE_VALUE - 512; // anything beyond this margin is a mate score, not a real eval

// ---------------------------------------------------------------------
// Transposition table, keyed on the Zobrist hash. Stores a fail-soft
// score bound (exact/lower/upper), the depth it was searched to, and the
// best move found — the latter matters even when the depth is too
// shallow for a cutoff, since it's still the strongest move-ordering hint
// available (searched first, so alpha-beta narrows the window fastest).
// ---------------------------------------------------------------------

enum TTFlag { TT_EXACT, TT_LOWERBOUND, TT_UPPERBOUND };

struct TTEntry{
    U64 key;
    int depth; // -1 marks an empty slot
    int score;
    uint32_t bestMove;
    TTFlag flag;
};

const size_t TT_SIZE = 1ULL << 20; // ~1M entries (~24MB)
extern TTEntry tt_table[TT_SIZE];

void tt_clear();
TTEntry* tt_probe(U64 key);

// depth-preferred replacement: never let a shallow re-search evict a
// deeper, more valuable entry for the same key
void tt_store(U64 key, int depth, int score, TTFlag flag, uint32_t bestMove);

// Mate scores encode distance-to-mate from the CURRENT node ("mate in N"),
// but that distance means something different at every ply from the root.
// Storing/retrieving through the TT re-bases the score onto the node
// where it's being stored or read, so a transposed path to the same
// subtree still reports the correct mate distance from wherever it sits.
int value_to_tt(int score, int ply);
int value_from_tt(int score, int ply);
