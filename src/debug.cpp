#include "debug.h"
#include "board.h"
#include <iostream>
#include <stdio.h>

void print_bitboard(U64 bitboard)
{
    printf("\n");

    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        // loop over board files
        for (int file = 0; file < 8; file++)
        {
            // init board square
            int square = rank * 8 + file;

            // print ranks
            if (!file)
                printf("  %d ", 8 - rank);

            // print bit indexed by board square
            printf(" %d", get_bit(bitboard, square) ? 1 : 0);
        }

        printf("\n");
    }

    // print files
    printf("\n     a b c d e f g h\n\n");

    // print bitboard as decimal
    printf("     bitboard: %llud\n\n", bitboard);
}

const char* piece_name(int piece) {
    switch (piece) {
        case P: return "P";
        case N: return "N";
        case B: return "B";
        case R: return "R";
        case Q: return "Q";
        case K: return "K";
        default: return "?";
    }
}

const char* color_name(int color) {
    return color == WHITE ? "WHITE" : "BLACK";
}

void print_square(int square, char out[3]) {
    out[0] = 'a' + (square % 8);
    out[1] = '8' - (square / 8);
    out[2] = '\0';
}

void print_move(uint32_t move) {
    int from = FROM(move);
    int to = TO(move);
    int promoted = (move >> 16) & 0xf;
    bool capture = (move >> 20) & 1;
    bool double_push = (move >> 21) & 1;
    bool enpassant = (move >> 22) & 1;
    bool castling = (move >> 23) & 1;

    char from_sq[3];
    char to_sq[3];
    print_square(from, from_sq);
    print_square(to, to_sq);

    std::cout << from_sq << "->" << to_sq;
    if (promoted) std::cout << "=" << piece_name(promoted);
    if (capture) std::cout << "x";
    if (double_push) std::cout << "d";
    if (enpassant) std::cout << "e";
    if (castling) std::cout << "c";
}

void print_move_list(const MoveList& list) {
    for (int i = 0; i < list.count; i++) {
        print_move(list.moves[i]);
        if (i + 1 < list.count) std::cout << ", ";
    }
    std::cout << "\n";
}
