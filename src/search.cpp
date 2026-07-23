#include "search.h"
#include "make_move.h"
#include "eval.h"
#include "tt.h"
#include "notation.h"
#include <cstring>
#include <iostream>
#include <algorithm>

bool g_stop = false;
uint64_t g_nodes = 0;
bool g_time_limited = false;
std::chrono::steady_clock::time_point g_deadline;

bool check_time(){
    g_nodes++;
    if(g_stop) return true;
    if(g_time_limited && (g_nodes & 2047) == 0){
        if(std::chrono::steady_clock::now() >= g_deadline) g_stop = true;
    }
    return g_stop;
}

uint32_t killer_moves[MAX_PLY][2];
int history_table[2][64][64];

void clear_search_heuristics(){
    memset(killer_moves, 0, sizeof(killer_moves));
    memset(history_table, 0, sizeof(history_table));
}

void age_history(){
    for(int c = 0; c < 2; c++)
        for(int f = 0; f < 64; f++)
            for(int t = 0; t < 64; t++)
                history_table[c][f][t] /= 2;
}

int mvv_lva_score(const Board &board, uint32_t move){
    bool capture = (move >> 20) & 1;
    if(!capture) return -1;

    bool enpassant = (move >> 22) & 1;
    int attacker = PIECE(move);

    int victim = P;
    if(!enpassant){
        int to = TO(move);
        int them = board.sideToMove ^ 1;
        for(int pt = P; pt <= K; pt++){
            if(board.pieces[them][pt] & (1ULL << to)){
                victim = pt;
                break;
            }
        }
    }

    static const int piece_value[6] = {100, 320, 330, 500, 900, 20000};
    return piece_value[victim] * 10 - piece_value[attacker];
}

void order_moves(const Board &board, MoveList &moves, uint32_t tt_move){
    std::sort(moves.moves, moves.moves + moves.count, [&](uint32_t a, uint32_t b){
        int sa = (tt_move && a == tt_move) ? INF_SCORE : mvv_lva_score(board, a);
        int sb = (tt_move && b == tt_move) ? INF_SCORE : mvv_lva_score(board, b);
        return sa > sb;
    });
}

// Full ordering priority, highest to lowest: TT move, then captures by
// MVV-LVA, then killer moves for this ply, then quiet moves by history
// score. Used only by the main search (negamax and the root) — quiescence
// only ever considers captures/evasions, so it has nothing for killers or
// history to distinguish and just uses the plain order_moves above.
int move_order_score(const Board &board, uint32_t move, int ply, uint32_t tt_move){
    if(tt_move && move == tt_move) return 2000000;

    bool capture = (move >> 20) & 1;
    if(capture) return 1000000 + mvv_lva_score(board, move);

    int killer_ply = std::min(ply, MAX_PLY - 1); // deep explicit-depth searches can exceed MAX_PLY
    if(move == killer_moves[killer_ply][0]) return 900000;
    if(move == killer_moves[killer_ply][1]) return 800000;

    return std::min(history_table[board.sideToMove][FROM(move)][TO(move)], 500000);
}

void order_moves_with_heuristics(const Board &board, MoveList &moves, int ply, uint32_t tt_move){
    std::sort(moves.moves, moves.moves + moves.count, [&](uint32_t a, uint32_t b){
        return move_order_score(board, a, ply, tt_move) > move_order_score(board, b, ply, tt_move);
    });
}

// Quiescence search: resolves tactical sequences (captures, promotions,
// and — if the side to move is in check — every legal reply) past the
// main search's depth cutoff, so the static eval is never sampled in the
// middle of a capture exchange. "Stand pat" reflects that a side is never
// forced to capture: if staying quiet already beats beta, or is the best
// option found so far, that's a valid alpha-beta bound in its own right.
// If in check there's no stand-pat option — sitting still isn't legal —
// so every legal move must be considered, mirroring negamax's mate logic.
int quiescence(Board &board, int ply, int alpha, int beta){
    if(check_time()) return 0;

    int king_sq = get_lsb(board.pieces[board.sideToMove][K]);
    bool in_check = is_square_attacked(king_sq, board.sideToMove ^ 1, board);

    if(!in_check){
        int e = evaluate(board);
        int stand_pat = (board.sideToMove == WHITE) ? e : -e;

        if(stand_pat >= beta) return beta;
        if(stand_pat > alpha) alpha = stand_pat;
    }

    MoveList moves = in_check ? generate_legal_moves(board) : generate_legal_captures(board);

    if(in_check && moves.count == 0) return -(MATE_VALUE - ply);

    order_moves(board, moves);

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        int score = -quiescence(board, ply + 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }

    return alpha;
}

int negamax(Board &board, int depth, int ply, int alpha, int beta){
    if(check_time()) return 0;

    if(depth == 0){
        return quiescence(board, ply, alpha, beta);
    }

    int alpha_orig = alpha;
    uint32_t tt_move = 0;

    TTEntry *entry = tt_probe(board.zobristKey);
    if(entry){
        tt_move = entry->bestMove;
        if(entry->depth >= depth){
            int tt_score = value_from_tt(entry->score, ply);
            if(entry->flag == TT_EXACT) return tt_score;
            if(entry->flag == TT_UPPERBOUND && tt_score <= alpha) return tt_score;
            if(entry->flag == TT_LOWERBOUND && tt_score >= beta) return tt_score;
        }
    }

    MoveList moves = generate_legal_moves(board);
    if(moves.count == 0){
        int king_sq = get_lsb(board.pieces[board.sideToMove][K]);
        bool in_check = is_square_attacked(king_sq, board.sideToMove ^ 1, board);
        if(in_check) return -(MATE_VALUE - ply); // checkmated; prefer surviving longer
        return 0; // stalemate
    }

    order_moves_with_heuristics(board, moves, ply, tt_move);

    int best = -INF_SCORE;
    uint32_t best_move = moves.moves[0];
    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        int score = -negamax(board, depth - 1, ply + 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score > best){
            best = score;
            best_move = moves.moves[i];
        }
        if(best > alpha) alpha = best;
        if(alpha >= beta){
            // a quiet move refuting this many siblings is worth remembering,
            // both for sibling nodes at this same ply (killers) and for
            // quiet moves sharing this from/to anywhere in the tree (history)
            bool capture = (moves.moves[i] >> 20) & 1;
            if(!capture){
                int killer_ply = std::min(ply, MAX_PLY - 1);
                if(moves.moves[i] != killer_moves[killer_ply][0]){
                    killer_moves[killer_ply][1] = killer_moves[killer_ply][0];
                    killer_moves[killer_ply][0] = moves.moves[i];
                }
                history_table[board.sideToMove][FROM(moves.moves[i])][TO(moves.moves[i])] += depth * depth;
            }
            break; // beta cutoff
        }
    }

    // if time ran out partway through this node's move loop, `best` may
    // reflect moves cut short by check_time()'s throwaway 0 — never let
    // that leak into the table
    if(!g_stop){
        TTFlag flag = (best <= alpha_orig) ? TT_UPPERBOUND : (best >= beta) ? TT_LOWERBOUND : TT_EXACT;
        tt_store(board.zobristKey, depth, value_to_tt(best, ply), flag, best_move);
    }

    return best;
}

uint32_t find_best_move(Board &board, int depth, int &out_score){
    MoveList moves = generate_legal_moves(board);

    TTEntry *entry = tt_probe(board.zobristKey);
    order_moves_with_heuristics(board, moves, 0, entry ? entry->bestMove : 0);

    int alpha = -INF_SCORE, beta = INF_SCORE;
    int best_score = -INF_SCORE;
    uint32_t best_move = 0;

    for(int i = 0; i < moves.count; i++){
        if(g_stop) break; // time's up mid-iteration; caller discards this depth's result anyway

        Undo undo = make_move(board, moves.moves[i]);
        int score = -negamax(board, depth - 1, 1, -beta, -alpha);
        unmake_move(board, moves.moves[i], undo);

        if(score > best_score){
            best_score = score;
            best_move = moves.moves[i];
        }
        if(best_score > alpha) alpha = best_score;
    }

    out_score = best_score;
    return best_move;
}

SearchInfo iterative_deepening(Board &board, int maxDepth, long long timeLimitMs, bool report){
    g_stop = false;
    g_nodes = 0;
    g_time_limited = (timeLimitMs >= 0);

    // killers are ply-indexed guesses shaped by THIS tree, so a previous
    // search's killers are irrelevant here; history generalizes better
    // across searches, so it's aged rather than wiped (see explanations.txt)
    memset(killer_moves, 0, sizeof(killer_moves));
    age_history();

    auto start = std::chrono::steady_clock::now();
    if(g_time_limited) g_deadline = start + std::chrono::milliseconds(timeLimitMs);

    SearchInfo result;

    for(int depth = 1; depth <= maxDepth; depth++){
        if(g_stop) break;

        int score;
        uint32_t move = find_best_move(board, depth, score);
        bool aborted = g_stop;

        if(aborted && depth > 1) break; // partial result at this depth is unreliable; keep the last complete one

        result.bestMove = move;
        result.score = score;
        result.depthReached = depth;
        result.nodes = g_nodes;

        if(report){
            long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            std::cout << "info depth " << depth << " score cp " << score
                       << " nodes " << g_nodes << " time " << elapsed
                       << " pv " << move_to_uci(move) << "\n" << std::flush;
        }

        if(aborted) break;
        if(score >= MATE_THRESHOLD || score <= -MATE_THRESHOLD) break; // forced mate found; deeper can't do better
    }

    return result;
}
