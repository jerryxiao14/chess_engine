#include "fen.h"
#include "zobrist.h"
#include <sstream>
#include <cctype>
#include <cassert>
#include <cstring>

void parse_fen(Board &board, const std::string &fen){
    memset(&board, 0, sizeof(Board));

    std::istringstream ss(fen);
    std::string boardPart, sidePart, castlePart, epPart;
    int halfmove = 0, fullmove = 1;

    ss >> boardPart >> sidePart >> castlePart >> epPart;
    if(!(ss >> halfmove)) halfmove = 0;
    if(!(ss >> fullmove)) fullmove = 1;

    int square = 0;
    for(char c : boardPart){
        if(c == '/') continue;
        if(isdigit((unsigned char)c)){
            square += c - '0';
            continue;
        }

        int color = isupper((unsigned char)c) ? WHITE : BLACK;
        int piece;
        switch(tolower(c)){
            case 'p': piece = P; break;
            case 'n': piece = N; break;
            case 'b': piece = B; break;
            case 'r': piece = R; break;
            case 'q': piece = Q; break;
            case 'k': piece = K; break;
            default: piece = -1; break;
        }
        assert(piece != -1);

        set_bit(board.pieces[color][piece], square);
        square++;
    }

    for(int c = WHITE; c <= BLACK; c++){
        for(int p = P; p <= K; p++){
            board.occupancies[c] |= board.pieces[c][p];
        }
    }
    board.occupancies[BOTH] = board.occupancies[WHITE] | board.occupancies[BLACK];

    board.sideToMove = (sidePart == "w") ? WHITE : BLACK;

    board.castlingRights = 0;
    if(castlePart.find('K') != std::string::npos) board.castlingRights |= WKCA;
    if(castlePart.find('Q') != std::string::npos) board.castlingRights |= WQCA;
    if(castlePart.find('k') != std::string::npos) board.castlingRights |= BKCA;
    if(castlePart.find('q') != std::string::npos) board.castlingRights |= BQCA;

    if(epPart == "-"){
        board.enPassant = NO_SQ;
    }
    else{
        int file = epPart[0] - 'a';
        int rank = epPart[1] - '0'; // 1..8, matching FEN rank digits
        board.enPassant = Square((8 - rank) * 8 + file);
    }

    board.halfmoveClock = halfmove;
    board.fullmoveNumber = fullmove;

    board.zobristKey = compute_zobrist_key(board);
}
