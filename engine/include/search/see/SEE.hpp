#ifndef BOSON_SEE_HPP
#define BOSON_SEE_HPP

#include "board/Position.hpp"
#include "board/Move.hpp"
#include <cstdint>

namespace Boson {

class SEE {
public:
    // Accept raw source and destination coordinates to bypass object packing layers
    static int evaluate(Position& pos, Square fromSq, Square toSq) noexcept;

private:
    static int getPieceValue(Piece p) noexcept;
    static Bitboard getAttackers(const Position& pos, Square target, Bitboard occupancy) noexcept;
    static Square getLeastValuableAttacker(const Position& pos, Bitboard attackers, Color side, Piece& outPiece) noexcept;
};

} // namespace Boson

#endif // BOSON_SEE_HPP