#ifndef BOSON_PIECE_HPP
#define BOSON_PIECE_HPP

#include <cstdint>

namespace Boson {

enum class PieceType : uint8_t {
Pawn = 0,
Knight = 1,
Bishop = 2,
Rook = 3,
Queen = 4,
King = 5,
None = 6
};

enum class Piece : uint8_t {
WhitePawn = 0, WhiteKnight = 1, WhiteBishop = 2, WhiteRook = 3, WhiteQueen = 4, WhiteKing = 5,
BlackPawn = 6, BlackKnight = 7, BlackBishop = 8, BlackRook = 9, BlackQueen = 10, BlackKing = 11,
None = 12
};

} // namespace Boson

#endif // BOSON_PIECE_HPP