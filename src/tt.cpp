#include "tt.h"

TTEntry tt_table[TT_SIZE];

void tt_clear(){
    for(size_t i = 0; i < TT_SIZE; i++) tt_table[i].depth = -1;
}

TTEntry* tt_probe(U64 key){
    TTEntry &e = tt_table[key & (TT_SIZE - 1)];
    if(e.depth >= 0 && e.key == key) return &e;
    return nullptr;
}

void tt_store(U64 key, int depth, int score, TTFlag flag, uint32_t bestMove){
    TTEntry &e = tt_table[key & (TT_SIZE - 1)];
    if(e.depth < 0 || e.key != key || depth >= e.depth){
        e.key = key;
        e.depth = depth;
        e.score = score;
        e.flag = flag;
        e.bestMove = bestMove;
    }
}

int value_to_tt(int score, int ply){
    if(score >= MATE_THRESHOLD) return score + ply;
    if(score <= -MATE_THRESHOLD) return score - ply;
    return score;
}

int value_from_tt(int score, int ply){
    if(score >= MATE_THRESHOLD) return score - ply;
    if(score <= -MATE_THRESHOLD) return score + ply;
    return score;
}
