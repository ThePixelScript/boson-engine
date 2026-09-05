#ifndef BOSON_SEE_HPP
#define BOSON_SEE_HPP

#include "board/Position.hpp"
#include "board/Move.hpp"
#include <cstdint>

namespace Boson {

class SEE {
public:
    static int evaluate(const Position& pos, Square fromSq, Square toSq) noexcept;
    static int evaluate(const Position& pos, Move move) noexcept {
        return evaluate(pos, move.getFromSquare(), move.getToSquare());
    }

private:
    static int getPieceValue(Piece p) noexcept;
    static Bitboard getAttackers(const Position& pos, Square target, Bitboard occupancy) noexcept;
    static Square getLeastValuableAttacker(const Position& pos, Bitboard attackers, Color side, Piece& outPiece) noexcept;
};

} // namespace Boson

#endif // BOSON_SEE_HPP