#include "make_move.h"
#include "zobrist.h"

Undo make_move(Board &board, uint32_t move){
    int from = FROM(move);
    int to = TO(move);
    int piece = PIECE(move);

    int promoted = (move >> 16)&0xf;

    bool capture = (move>>20)&1;
    bool double_push = (move>>21) &1;
    bool enpassant = (move>>22) & 1;
    bool castling = (move >>23 &1);
    int color = board.sideToMove;
    int us = color;
    int them = color ^1;

    Undo undo;
    undo.capturedPiece = -1;
    undo.castlingRights = board.castlingRights;
    undo.enPassant = board.enPassant;
    undo.halfmoveClock = board.halfmoveClock;
    undo.fullmoveNumber = board.fullmoveNumber;
    undo.zobristKey = board.zobristKey;

    int enpassant_capture_sq = -1;
    // clear moving piece from square "from"
    board.pieces[us][piece] &= ~(1ULL << from);
    board.occupancies[us] &= ~(1ULL << from);
    board.occupancies[BOTH] &= ~(1ULL << from);

    // handle capture
    if(capture){
        if(enpassant){
            if(color==WHITE) enpassant_capture_sq = to+8;
            else enpassant_capture_sq = to-8;

            board.pieces[them][P] &= ~(1ULL<<enpassant_capture_sq);
            board.occupancies[them] &= ~(1ULL<<enpassant_capture_sq);
            board.occupancies[BOTH] &= ~(1ULL << enpassant_capture_sq);
        }
        else{
            for(int pt = P;pt<=K;pt++){
                if(board.pieces[them][pt] & (1ULL<<to)){
                    undo.capturedPiece = pt;
                    board.pieces[them][pt] &= ~(1ULL<<to);
                    board.occupancies[them] &= ~(1ULL <<to);
                    break;
                }
            }
        }

        if(them==WHITE){
            if(to==h1) board.castlingRights &= ~WKCA;
            else if(to==a1) board.castlingRights &= ~WQCA;
        }
        else{
            if(to==h8) board.castlingRights &= ~BKCA;
            else if (to== a8) board.castlingRights &= ~BQCA;
        }
    }

    // plac moved piece on to
    if(promoted){
        board.pieces[us][promoted] |= (1ULL <<to);
    }
    else{
        board.pieces[us][piece] |= (1ULL << to);
    }

    board.occupancies[us] |= (1ULL<<to);
    board.occupancies[BOTH] |= (1ULL<<to);

    if(double_push){
        if(color==WHITE) board.enPassant = Square(to+8);
        else board.enPassant = Square(to-8);
    }
    else board.enPassant = NO_SQ;

    // handle moving rook too for castling
    if(castling){
        // white kingside castling
        if(to==g1){
            board.pieces[us][R] &= ~(1ULL<<(h1));
            board.occupancies[us] &= ~(1ULL<<(h1));
            board.occupancies[BOTH] &= ~(1ULL<<(h1));

            board.pieces[us][R] |= (1ULL<<(f1));
            board.occupancies[us] |= (1ULL<<(f1));
            board.occupancies[BOTH] |= (1ULL<<(f1));
        }
        // white queenside castling
        else if(to==c1){
            board.pieces[us][R] &= ~(1ULL<<(a1));
            board.occupancies[us] &= ~(1ULL<<(a1));
            board.occupancies[BOTH] &= ~(1ULL<<(a1));

            board.pieces[us][R] |= (1ULL<<(d1));
            board.occupancies[us] |= (1ULL<<(d1));
            board.occupancies[BOTH] |= (1ULL<<(d1));
        }
        else if(to==g8){
            board.pieces[us][R] &= ~(1ULL<<(h8));
            board.occupancies[us] &= ~(1ULL<<(h8));
            board.occupancies[BOTH] &= ~(1ULL<<(h8));

            board.pieces[us][R] |= (1ULL<<(f8));
            board.occupancies[us] |= (1ULL<<(f8));
            board.occupancies[BOTH] |= (1ULL<<(f8));
        }
        else{
            board.pieces[us][R] &= ~(1ULL<<(a8));
            board.occupancies[us] &= ~(1ULL<<(a8));
            board.occupancies[BOTH] &= ~(1ULL<<(a8));

            board.pieces[us][R] |= (1ULL<<(d8));
            board.occupancies[us] |= (1ULL<<(d8));
            board.occupancies[BOTH] |= (1ULL<<(d8));
        }
    }

    // castling rights
    if(piece==K){
        if(us==WHITE) board.castlingRights &= ~(WKCA | WQCA);
        else board.castlingRights &= ~(BKCA | BQCA);
    }

    if(piece==R){
        if(us==WHITE){
            if(from==h1) board.castlingRights &= ~WKCA;
            else if(from==a1) board.castlingRights &= ~WQCA;
        }
        else{
            if(from==h8) board.castlingRights &= ~BKCA;
            else if(from==a8) board.castlingRights &= ~BQCA;
        }
    }

    if(piece==P || capture) board.halfmoveClock=0;
    else board.halfmoveClock++;

    if(color==BLACK) board.fullmoveNumber++;

    board.sideToMove = (Color)them;

    // incremental zobrist update
    board.zobristKey ^= piece_keys[us][piece][from];
    board.zobristKey ^= piece_keys[us][promoted ? promoted : piece][to];

    if(capture){
        if(enpassant) board.zobristKey ^= piece_keys[them][P][enpassant_capture_sq];
        else board.zobristKey ^= piece_keys[them][undo.capturedPiece][to];
    }

    if(castling){
        if(to==g1) board.zobristKey ^= piece_keys[us][R][h1] ^ piece_keys[us][R][f1];
        else if(to==c1) board.zobristKey ^= piece_keys[us][R][a1] ^ piece_keys[us][R][d1];
        else if(to==g8) board.zobristKey ^= piece_keys[us][R][h8] ^ piece_keys[us][R][f8];
        else board.zobristKey ^= piece_keys[us][R][a8] ^ piece_keys[us][R][d8];
    }

    board.zobristKey ^= castling_keys[undo.castlingRights] ^ castling_keys[board.castlingRights];
    board.zobristKey ^= ep_zobrist(undo.enPassant) ^ ep_zobrist(board.enPassant);
    board.zobristKey ^= side_key;

    return undo;
}

void unmake_move(Board &board, uint32_t move, const Undo &undo){
    int from = FROM(move);
    int to = TO(move);
    int piece = PIECE(move);

    int promoted = (move >> 16)&0xf;

    bool capture = (move>>20)&1;
    bool enpassant = (move>>22) & 1;
    bool castling = (move >>23 &1);

    board.sideToMove = (Color)(board.sideToMove ^ 1);
    int us = board.sideToMove;
    int them = us ^ 1;

    // undo castling rook move
    if(castling){
        if(to==g1){
            board.pieces[us][R] &= ~(1ULL<<f1);
            board.occupancies[us] &= ~(1ULL<<f1);
            board.occupancies[BOTH] &= ~(1ULL<<f1);

            board.pieces[us][R] |= (1ULL<<h1);
            board.occupancies[us] |= (1ULL<<h1);
            board.occupancies[BOTH] |= (1ULL<<h1);
        }
        else if(to==c1){
            board.pieces[us][R] &= ~(1ULL<<d1);
            board.occupancies[us] &= ~(1ULL<<d1);
            board.occupancies[BOTH] &= ~(1ULL<<d1);

            board.pieces[us][R] |= (1ULL<<a1);
            board.occupancies[us] |= (1ULL<<a1);
            board.occupancies[BOTH] |= (1ULL<<a1);
        }
        else if(to==g8){
            board.pieces[us][R] &= ~(1ULL<<f8);
            board.occupancies[us] &= ~(1ULL<<f8);
            board.occupancies[BOTH] &= ~(1ULL<<f8);

            board.pieces[us][R] |= (1ULL<<h8);
            board.occupancies[us] |= (1ULL<<h8);
            board.occupancies[BOTH] |= (1ULL<<h8);
        }
        else{
            board.pieces[us][R] &= ~(1ULL<<d8);
            board.occupancies[us] &= ~(1ULL<<d8);
            board.occupancies[BOTH] &= ~(1ULL<<d8);

            board.pieces[us][R] |= (1ULL<<a8);
            board.occupancies[us] |= (1ULL<<a8);
            board.occupancies[BOTH] |= (1ULL<<a8);
        }
    }

    // remove the moved (or promoted) piece from "to"
    if(promoted){
        board.pieces[us][promoted] &= ~(1ULL<<to);
    }
    else{
        board.pieces[us][piece] &= ~(1ULL<<to);
    }
    board.occupancies[us] &= ~(1ULL<<to);
    board.occupancies[BOTH] &= ~(1ULL<<to);

    // restore whatever was captured
    if(capture){
        if(enpassant){
            int cap_sq = (us==WHITE) ? to+8 : to-8;
            board.pieces[them][P] |= (1ULL<<cap_sq);
            board.occupancies[them] |= (1ULL<<cap_sq);
            board.occupancies[BOTH] |= (1ULL<<cap_sq);
        }
        else{
            board.pieces[them][undo.capturedPiece] |= (1ULL<<to);
            board.occupancies[them] |= (1ULL<<to);
            board.occupancies[BOTH] |= (1ULL<<to);
        }
    }

    // put the moving piece back on "from"
    board.pieces[us][piece] |= (1ULL<<from);
    board.occupancies[us] |= (1ULL<<from);
    board.occupancies[BOTH] |= (1ULL<<from);

    board.castlingRights = undo.castlingRights;
    board.enPassant = undo.enPassant;
    board.halfmoveClock = undo.halfmoveClock;
    board.fullmoveNumber = undo.fullmoveNumber;
    board.zobristKey = undo.zobristKey;
}
