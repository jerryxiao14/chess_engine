// ---------------------------------------------------------------------
// Evaluation: material + piece-square tables, based on Tomasz
// Michniewski's "Simplified Evaluation Function"
// (https://www.chessprogramming.org/Simplified_Evaluation_Function).
// Tables are written rank8-to-rank1, top-to-bottom, which is exactly our
// square numbering (a8=0 ... h1=63), so they're used as-is for WHITE;
// BLACK looks up the same table with the square mirrored vertically
// (sq ^ 56 flips rank_index while leaving the file untouched, since
// square = rank_index*8+file and 56 = 0b111000).
//
// The king table is phase-tapered (PeSTO-style, MAX_PHASE=24 based on
// remaining non-pawn material) between a middlegame table that rewards
// castled safety and an endgame table that rewards a centralized,
// active king — the one piece whose ideal placement flips hardest
// between game phases.
// ---------------------------------------------------------------------

#include "eval.h"
#include <algorithm>

static const int material_value[6] = {100, 320, 330, 500, 900, 0};

static const int pawn_pst[64] = {
     0,   0,   0,   0,   0,   0,  0,   0,
    50,  50,  50,  50,  50,  50, 50,  50,
    10,  10,  20,  30,  30,  20, 10,  10,
     5,   5,  10,  25,  25,  10,  5,   5,
     0,   0,   0,  20,  20,   0,  0,   0,
     5,  -5, -10,   0,   0, -10, -5,   5,
     5,  10,  10, -20, -20,  10, 10,   5,
     0,   0,   0,   0,   0,   0,  0,   0,
};

static const int knight_pst[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

static const int bishop_pst[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

static const int rook_pst[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     5,  10,  10,  10,  10,  10,  10,   5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
    -5,   0,   0,   0,   0,   0,   0,  -5,
     0,   0,   0,   5,   5,   0,   0,   0,
};

static const int queen_pst[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};

static const int king_mg_pst[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20,
};

static const int king_eg_pst[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -30, -20, -10,   0,   0, -10, -20, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  30,  40,  40,  30, -10, -30,
    -30, -10,  20,  30,  30,  20, -10, -30,
    -30, -30,   0,   0,   0,   0, -30, -30,
    -50, -30, -30, -30, -30, -30, -30, -50,
};

static const int piece_phase_weight[6] = {0, 1, 1, 2, 4, 0}; // P,N,B,R,Q,K
static const int MAX_PHASE = 24; // 2 * (2*N + 2*B + 2*R*2 + Q*4) per side

static inline int pst_value(int piece, int sq){
    switch(piece){
        case P: return pawn_pst[sq];
        case N: return knight_pst[sq];
        case B: return bishop_pst[sq];
        case R: return rook_pst[sq];
        case Q: return queen_pst[sq];
        default: return 0;
    }
}

static int compute_phase(const Board &board){
    int phase = 0;
    for(int c = WHITE; c <= BLACK; c++){
        for(int p = N; p <= Q; p++){
            phase += piece_phase_weight[p] * __builtin_popcountll(board.pieces[c][p]);
        }
    }
    return std::min(phase, MAX_PHASE);
}

int evaluate(const Board &board){
    int phase = compute_phase(board);
    int score = 0;

    for(int c = WHITE; c <= BLACK; c++){
        int sign = (c == WHITE) ? 1 : -1;

        for(int p = P; p <= K; p++){
            U64 bb = board.pieces[c][p];
            while(bb){
                int sq = pop_lsb(bb);
                int pstSq = (c == WHITE) ? sq : (sq ^ 56);

                int value = material_value[p];
                if(p == K){
                    value += (king_mg_pst[pstSq] * phase + king_eg_pst[pstSq] * (MAX_PHASE - phase)) / MAX_PHASE;
                } else {
                    value += pst_value(p, pstSq);
                }

                score += sign * value;
            }
        }
    }

    return score;
}
