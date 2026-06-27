#ifndef BOSON_PIECE_SQUARE_TABLES_HPP
#define BOSON_PIECE_SQUARE_TABLES_HPP

#include <array>
#include <cstdint>

namespace Boson {

class PieceSquareTables {
public:
    // Positional preference arrays from White's perspective
    static const std::array<int16_t, 64> Pawn;
    static const std::array<int16_t, 64> Knight;
    static const std::array<int16_t, 64> Bishop;
    static const std::array<int16_t, 64> Rook;
    static const std::array<int16_t, 64> Queen;
    static const std::array<int16_t, 64> KingMiddle;
};

} // namespace Boson

#endif // BOSON_PIECE_SQUARE_TABLES_HPP