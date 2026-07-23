#include "board.h"
#include "attacks.h"
#include "zobrist.h"
#include "make_move.h"
#include "movegen.h"
#include "fen.h"
#include "eval.h"
#include "tt.h"
#include "search.h"
#include "perft.h"
#include "debug.h"
#include "uci.h"

#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char** argv){
    init_knight_attacks();
    init_king_attacks();
    init_pawn_attacks();

    init_slider_masks();
    initialize_relevancy_bits();

    init_rook_magic_table();
    init_bishop_magic_table();

    init_zobrist();
    tt_clear();
    clear_search_heuristics();

    if(argc >= 3 && std::string(argv[1]) == "eval"){
        Board board;
        parse_fen(board, argv[2]);
        std::cout << evaluate(board) << "\n";
        return 0;
    }

    if(argc >= 4 && std::string(argv[1]) == "bestmove"){
        Board board;
        parse_fen(board, argv[2]);
        int depth = std::atoi(argv[3]);

        SearchInfo info = iterative_deepening(board, depth, -1, false);

        print_move(info.bestMove);
        std::cout << "  score: " << info.score << "  depth: " << info.depthReached
                   << "  nodes: " << info.nodes << "\n";
        return 0;
    }

    if(argc >= 3 && std::string(argv[1]) == "key"){
        Board board;
        parse_fen(board, argv[2]);
        std::cout << std::hex << board.zobristKey << std::dec << "\n";
        return 0;
    }

    if(argc >= 4 && (std::string(argv[1]) == "divide" || std::string(argv[1]) == "perft" || std::string(argv[1]) == "zobrist")){
        std::string mode = argv[1];
        std::string fen = argv[2];
        int depth = std::atoi(argv[3]);

        Board board;
        parse_fen(board, fen);

        if(mode == "divide") perft_divide(board, depth);
        else if(mode == "zobrist") run_zobrist_test(fen, depth);
        else std::cout << perft(board, depth) << "\n";

        return 0;
    }

    if(argc >= 2 && std::string(argv[1]) == "test"){
        const std::string kiwipete =
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

        std::cout << "startpos:\n";
        run_perft_test(START_FEN, 1, 20);
        run_perft_test(START_FEN, 2, 400);
        run_perft_test(START_FEN, 3, 8902);
        run_perft_test(START_FEN, 4, 197281);
        run_perft_test(START_FEN, 5, 4865609);
        run_zobrist_test(START_FEN, 5);

        std::cout << "\nkiwipete:\n";
        run_perft_test(kiwipete, 1, 48);
        run_perft_test(kiwipete, 2, 2039);
        run_perft_test(kiwipete, 3, 97862);
        run_perft_test(kiwipete, 4, 4085603);
        run_zobrist_test(kiwipete, 4);

        return 0;
    }

    // no recognized subcommand: behave like a normal UCI engine binary,
    // which is what GUIs expect when they launch it with no arguments
    uci_loop();
    return 0;
}
