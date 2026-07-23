// ---------------------------------------------------------------------
// UCI protocol
// ---------------------------------------------------------------------

#include "uci.h"
#include "notation.h"
#include "fen.h"
#include "movegen.h"
#include "make_move.h"
#include "search.h"
#include "tt.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const int DEFAULT_SEARCH_DEPTH = 6;

uint32_t find_move_by_uci(Board &board, const std::string &uci){
    int from = square_from_uci(uci.substr(0, 2));
    int to = square_from_uci(uci.substr(2, 2));

    int promo = 0;
    if(uci.size() >= 5){
        switch(tolower((unsigned char)uci[4])){
            case 'q': promo = Q; break;
            case 'r': promo = R; break;
            case 'b': promo = B; break;
            case 'n': promo = N; break;
        }
    }

    MoveList moves = generate_legal_moves(board);
    for(int i = 0; i < moves.count; i++){
        uint32_t mv = moves.moves[i];
        if((int)FROM(mv) == from && (int)TO(mv) == to && (int)((mv >> 16) & 0xf) == promo){
            return mv;
        }
    }
    return 0;
}

void uci_loop(){
    Board board;
    parse_fen(board, START_FEN);

    std::string line;
    while(std::getline(std::cin, line)){
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if(cmd == "uci"){
            std::cout << "id name ChessEngine\n";
            std::cout << "id author Jerry\n";
            std::cout << "uciok\n" << std::flush;
        }
        else if(cmd == "isready"){
            std::cout << "readyok\n" << std::flush;
        }
        else if(cmd == "ucinewgame"){
            tt_clear();
            clear_search_heuristics();
            parse_fen(board, START_FEN);
        }
        else if(cmd == "position"){
            std::string token;
            iss >> token;

            if(token == "startpos"){
                parse_fen(board, START_FEN);
            }
            else if(token == "fen"){
                std::string fen, part;
                for(int i = 0; i < 6 && (iss >> part); i++){
                    if(i) fen += " ";
                    fen += part;
                }
                parse_fen(board, fen);
            }

            std::string movesToken;
            if(iss >> movesToken && movesToken == "moves"){
                std::string moveStr;
                while(iss >> moveStr){
                    uint32_t mv = find_move_by_uci(board, moveStr);
                    if(mv) make_move(board, mv);
                }
            }
        }
        else if(cmd == "go"){
            bool explicitDepth = false;
            int depthArg = 0;
            long long movetime = -1, wtime = -1, btime = -1, winc = 0, binc = 0;

            std::string token;
            while(iss >> token){
                if(token == "depth"){ iss >> depthArg; explicitDepth = true; }
                else if(token == "movetime") iss >> movetime;
                else if(token == "wtime") iss >> wtime;
                else if(token == "btime") iss >> btime;
                else if(token == "winc") iss >> winc;
                else if(token == "binc") iss >> binc;
                // movestogo/ponder/infinite/searchmoves intentionally unhandled
            }

            int maxDepth;
            long long timeLimitMs = -1; // -1 = unlimited

            if(explicitDepth){
                // an explicit depth request searches exactly that depth via
                // iterative deepening, uninterrupted by the clock
                maxDepth = depthArg;
            }
            else if(movetime >= 0){
                maxDepth = 64;
                timeLimitMs = std::max(movetime - 50, 50LL); // 50ms safety margin for UCI I/O overhead
            }
            else if(wtime >= 0 || btime >= 0){
                maxDepth = 64;
                long long myTime = std::max((board.sideToMove == WHITE) ? wtime : btime, 0LL);
                long long myInc  = (board.sideToMove == WHITE) ? winc : binc;

                // simple fixed-fraction budget: assume ~20 moves left, never
                // risk more than half the clock, always leave enough for
                // at least a near-instant move
                long long budget = myTime / 20 + myInc;
                budget = std::min(budget, myTime / 2);
                budget = std::max(budget, 50LL);
                timeLimitMs = budget;
            }
            else{
                // bare "go" (or "go infinite", which we can't truly honor
                // without a search thread to interrupt): fall back to a
                // fixed depth
                maxDepth = DEFAULT_SEARCH_DEPTH;
            }

            SearchInfo info = iterative_deepening(board, maxDepth, timeLimitMs, true);
            std::cout << "bestmove " << (info.bestMove ? move_to_uci(info.bestMove) : "0000") << "\n" << std::flush;
        }
        else if(cmd == "stop"){
            // search is synchronous and already finished by the time "go"
            // returns, so there's nothing to interrupt
        }
        else if(cmd == "quit"){
            break;
        }
        // unrecognized commands are ignored, per the UCI convention
    }
}
