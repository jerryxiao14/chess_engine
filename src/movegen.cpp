#include "movegen.h"
#include "attacks.h"
#include "make_move.h"

void generate_knight_moves(MoveList& list,Board& board, int color){
    U64 knights = board.pieces[color][N];
    while(knights){
        int from = pop_lsb(knights);

        U64 attacks = knight_attacks[from];

        attacks &= (~board.occupancies[color]);

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,N,0,capture,0,0,0));
        }
    }
}

void generate_bishop_moves(MoveList& list, Board& board, int color){
    U64 bishops = board.pieces[color][B];

    while(bishops){
        int from = pop_lsb(bishops);
        U64 attacks = get_bishop_attacks(from,board.occupancies[BOTH]);

        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,B,0,capture,0,0,0));
        }
    }
}

void generate_rook_moves(MoveList& list, Board &board, int color){
    U64 rooks = board.pieces[color][R];

    while(rooks){
        int from = pop_lsb(rooks);
        U64 attacks = get_rook_attacks(from,board.occupancies[BOTH]);
        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);

            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,R,0,capture,0,0,0));
        }
    }
}

void generate_queen_moves(MoveList& list, Board& board, int color){
    U64 queens = board.pieces[color][Q];

    while(queens){
        int from = pop_lsb(queens);

        U64 attacks = get_rook_attacks(from,board.occupancies[BOTH])|get_bishop_attacks(from,board.occupancies[BOTH]);
        attacks &= ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);
            bool capture = get_bit(board.occupancies[color^1],to);

            add_move(list,encode_move(from,to,Q,0,capture,0,0,0));
        }
    }
}

void generate_pawn_moves(MoveList &list, Board &board, int color){
    U64 pawns = board.pieces[color][P];
    U64 enemy = board.occupancies[color^1];
    U64 empty = ~board.occupancies[BOTH];

    auto handlePromotion = [&](int from,int to,int capture){
        add_move(list,encode_move(from,to,P,Q,capture,0,0,0));
        add_move(list,encode_move(from,to,P,R,capture,0,0,0));
        add_move(list,encode_move(from,to,P,B,capture,0,0,0));
        add_move(list,encode_move(from,to,P,N,capture,0,0,0));
    };
    int from;
    int middle_to;
    int capture;
    int to;

    // En passant
    if(board.enPassant!=NO_SQ){
        to = board.enPassant;
        if(color==WHITE){
            from = to+7;
            if(from%8!=7 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }

            from = to+9;
            if(from%8!=0 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }
        }
        else{
            from = to-7;
            if(from%8!=0 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }

            from = to-9;
            if(from%8!=7 && (pawns&(1ULL<<from))){
                add_move(list,encode_move(from,to,P,0,1,0,1,0));
            }
        }
    }

    while(pawns){
        from = pop_lsb(pawns);

        // single pushes
        capture = 0;
        if(color==WHITE) to = from-8;
        else to = from+8;
        // if the square moving to is empty, then its a pseudo-legal move
        if((1ULL<<to)&empty){
            if(color==WHITE&&to<8) handlePromotion(from,to,capture);
            else if(color==BLACK&&to>=56) handlePromotion(from,to,capture);
            else add_move(list,encode_move(from,to,P,0,0,0,0,0));
        }

        //double push, has to have started from original pawn square
        if(color==WHITE&&(from>=48&&from<=55)) {
            to = from-16;
            middle_to = from-8;
            if(((1ULL<<to)&empty)>0 && ((1ULL<<middle_to)&empty)>0){
                add_move(list,encode_move(from,to,P,0,0,1,0,0));
            }
        }
        else if(color==BLACK&&(from>=8&&from<=15))
        {
            to = from+16;
            middle_to = from+8;
            if(((1ULL<<to)&empty)>0 && ((1ULL<<middle_to)&empty)>0){
                add_move(list,encode_move(from,to,P,0,0,1,0,0));
            }
        }

        // captures go diagonal
        if(color==WHITE){
            // goes -7 (capture right) and -9 (capture left)
            to = from-7;
            if(from%8 !=7 && (1ULL<<to)&enemy){
                if(to<8) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }

            to = from-9;
            if(from%8!=0 && (1ULL<<to)&enemy){
                if(to<8) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }
        }
        else{
            to = from+7;
            if(from%8 !=0 && (1ULL<<to)&enemy){
                if(to>=56) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }

            to = from+9;
            if(from%8!=7 && (1ULL<<to)&enemy){
                if(to>=56) handlePromotion(from,to,1);
                else add_move(list,encode_move(from,to,P,0,1,0,0,0));
            }
        }

    }
}

void generate_king_moves(MoveList &list, Board &board, int color){
    U64 kings = board.pieces[color][K];
    while(kings){
        int from = pop_lsb(kings);

        // normal attacks (one square)

        U64 attacks = king_attacks[from] & ~board.occupancies[color];

        while(attacks){
            int to = pop_lsb(attacks);
            bool capture = (board.occupancies[color^1]&(1ULL<<to))!=0;
            add_move(list,encode_move(from,to,K,0,capture,0,0,0));
        }

        // castling: squares between must be empty, and king may not start in,
        // pass through, or land on an attacked square
        if(color==WHITE){
            if (from == e1){
                if((board.castlingRights & WKCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<f1)|(1ULL<<g1))) &&
                   !is_square_attacked(e1,BLACK,board) &&
                   !is_square_attacked(f1,BLACK,board) &&
                   !is_square_attacked(g1,BLACK,board)){
                    add_move(list,encode_move(e1,g1,K,0,0,0,0,1));
                }

                if((board.castlingRights & WQCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<d1)|(1ULL<<c1)|(1ULL<<b1))) &&
                   !is_square_attacked(e1,BLACK,board) &&
                   !is_square_attacked(d1,BLACK,board) &&
                   !is_square_attacked(c1,BLACK,board)){
                    add_move(list,encode_move(e1,c1,K,0,0,0,0,1));
                }
            }
        }
        else{
            if(from==e8){
                if((board.castlingRights & BKCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<f8)|(1ULL<<g8))) &&
                   !is_square_attacked(e8,WHITE,board) &&
                   !is_square_attacked(f8,WHITE,board) &&
                   !is_square_attacked(g8,WHITE,board)){
                    add_move(list,encode_move(e8,g8,K,0,0,0,0,1));
                }

                if((board.castlingRights & BQCA) &&
                   !(board.occupancies[BOTH] & ((1ULL<<d8)|(1ULL<<c8)|(1ULL<<b8))) &&
                   !is_square_attacked(e8,WHITE,board) &&
                   !is_square_attacked(d8,WHITE,board) &&
                   !is_square_attacked(c8,WHITE,board)){
                    add_move(list,encode_move(e8,c8,K,0,0,0,0,1));
                }
            }
        }
    }
}

bool is_square_attacked(int square, int attackerColor, const Board& board) {
    if (attackerColor == WHITE) {
        if (pawn_attacks[BLACK][square] & board.pieces[WHITE][P]) return true;
        if (knight_attacks[square] & board.pieces[WHITE][N]) return true;
        if (king_attacks[square] & board.pieces[WHITE][K]) return true;
        if (get_bishop_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[WHITE][B] | board.pieces[WHITE][Q])) return true;
        if (get_rook_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[WHITE][R] | board.pieces[WHITE][Q])) return true;
    } else {
        if (pawn_attacks[WHITE][square] & board.pieces[BLACK][P]) return true;
        if (knight_attacks[square] & board.pieces[BLACK][N]) return true;
        if (king_attacks[square] & board.pieces[BLACK][K]) return true;
        if (get_bishop_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[BLACK][B] | board.pieces[BLACK][Q])) return true;
        if (get_rook_attacks(square, board.occupancies[BOTH]) &
            (board.pieces[BLACK][R] | board.pieces[BLACK][Q])) return true;
    }

    return false;
}

void generate_moves(MoveList &list, Board &board, int color){
    list.count = 0;
    generate_pawn_moves(list,board,color);
    generate_knight_moves(list,board,color);
    generate_bishop_moves(list,board,color);
    generate_rook_moves(list,board,color);
    generate_queen_moves(list,board,color);
    generate_king_moves(list,board,color);
}

MoveList generate_legal_moves(Board &board){
    MoveList legal; legal.count = 0;
    MoveList pseudo; pseudo.count = 0;

    generate_moves(pseudo,board,board.sideToMove);

    for(int i=0;i<pseudo.count;i++){
        uint32_t move = pseudo.moves[i];

        Undo undo = make_move(board,move);

        int king_sq = get_lsb(board.pieces[board.sideToMove^1][K]);
        if(!is_square_attacked(king_sq,board.sideToMove,board)){
            legal.moves[legal.count++] = move;
        }
        unmake_move(board,move,undo);
    }

    return legal;
}

MoveList generate_legal_captures(Board &board){
    MoveList legal; legal.count = 0;
    MoveList pseudo; pseudo.count = 0;

    generate_moves(pseudo,board,board.sideToMove);

    for(int i=0;i<pseudo.count;i++){
        uint32_t move = pseudo.moves[i];

        bool capture = (move>>20)&1;
        int promoted = (move>>16)&0xf;
        if(!capture && !promoted) continue;

        Undo undo = make_move(board,move);

        int king_sq = get_lsb(board.pieces[board.sideToMove^1][K]);
        if(!is_square_attacked(king_sq,board.sideToMove,board)){
            legal.moves[legal.count++] = move;
        }
        unmake_move(board,move,undo);
    }

    return legal;
}
