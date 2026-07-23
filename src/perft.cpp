#include "perft.h"
#include "movegen.h"
#include "make_move.h"
#include "zobrist.h"
#include "fen.h"
#include "debug.h"
#include <iostream>

uint64_t perft(Board &board, int depth){
    if(depth == 0) return 1ULL;

    MoveList moves = generate_legal_moves(board);
    uint64_t nodes = 0;

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        nodes += perft(board, depth - 1);
        unmake_move(board, moves.moves[i], undo);
    }

    return nodes;
}

void perft_divide(Board &board, int depth){
    MoveList moves = generate_legal_moves(board);
    uint64_t total = 0;

    for(int i = 0; i < moves.count; i++){
        uint32_t move = moves.moves[i];
        Undo undo = make_move(board, move);
        uint64_t nodes = perft(board, depth - 1);
        unmake_move(board, move, undo);

        print_move(move);
        std::cout << ": " << nodes << "\n";
        total += nodes;
    }

    std::cout << "Total: " << total << "\n";
}

void run_perft_test(const std::string &fen, int depth, uint64_t expected){
    Board board;
    parse_fen(board, fen);

    uint64_t nodes = perft(board, depth);
    std::cout << "perft(" << depth << ") = " << nodes
               << "  expected " << expected
               << (nodes == expected ? "  OK" : "  MISMATCH") << "\n";
}

uint64_t verify_zobrist(Board &board, int depth, int &mismatches){
    if(board.zobristKey != compute_zobrist_key(board)){
        mismatches++;
        std::cout << "zobrist MISMATCH at depth " << depth << "\n";
    }

    if(depth == 0) return 1ULL;

    MoveList moves = generate_legal_moves(board);
    uint64_t nodes = 1ULL;

    for(int i = 0; i < moves.count; i++){
        Undo undo = make_move(board, moves.moves[i]);
        nodes += verify_zobrist(board, depth - 1, mismatches);
        unmake_move(board, moves.moves[i], undo);
    }

    return nodes;
}

void run_zobrist_test(const std::string &fen, int depth){
    Board board;
    parse_fen(board, fen);

    int mismatches = 0;
    uint64_t nodes = verify_zobrist(board, depth, mismatches);
    std::cout << "zobrist check to depth " << depth << ": " << nodes
               << " nodes, " << mismatches << " mismatches"
               << (mismatches == 0 ? "  OK" : "  FAILED") << "\n";
}
