#include "notation.h"
#include "board.h"

int square_from_uci(const std::string &s){
    int file = s[0] - 'a';
    int rank = s[1] - '0'; // 1..8
    return (8 - rank) * 8 + file;
}

std::string uci_from_square(int sq){
    std::string s;
    s += char('a' + sq % 8);
    s += char('0' + (8 - sq / 8));
    return s;
}

std::string move_to_uci(uint32_t move){
    std::string s = uci_from_square(FROM(move)) + uci_from_square(TO(move));
    switch((move >> 16) & 0xf){
        case Q: s += 'q'; break;
        case R: s += 'r'; break;
        case B: s += 'b'; break;
        case N: s += 'n'; break;
        default: break;
    }
    return s;
}
