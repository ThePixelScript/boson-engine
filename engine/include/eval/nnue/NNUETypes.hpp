#ifndef BOSON_NNUE_TYPES_HPP
#define BOSON_NNUE_TYPES_HPP

#include "board/Piece.hpp"
#include "board/Square.hpp"
#include "board/Color.hpp"
#include <cstdint>

namespace Boson::eval::nnue {

constexpr int HALFKP_FEATURES = 40960;
constexpr int ACCUMULATOR_SIZE = 512;
constexpr int LAYER1_SIZE = 1024;
constexpr int HIDDEN_SIZE = 32;

enum class HalfKPPiece : int8_t {
    WhitePawn = 0,
    WhiteKnight = 1,
    WhiteBishop = 2,
    WhiteRook = 3,
    WhiteQueen = 4,
    BlackPawn = 5,
    BlackKnight = 6,
    BlackBishop = 7,
    BlackRook = 8,
    BlackQueen = 9,
    None = -1
};

[[nodiscard]] constexpr int pieceToHalfKP(Piece piece) noexcept {
    switch (piece) {
        case Piece::WhitePawn:   return 0;
        case Piece::WhiteKnight: return 1;
        case Piece::WhiteBishop: return 2;
        case Piece::WhiteRook:   return 3;
        case Piece::WhiteQueen:  return 4;
        case Piece::BlackPawn:   return 5;
        case Piece::BlackKnight: return 6;
        case Piece::BlackBishop: return 7;
        case Piece::BlackRook:   return 8;
        case Piece::BlackQueen:  return 9;
        default:                 return -1;
    }
}

[[nodiscard]] constexpr HalfKPPiece toHalfKPPiece(Piece piece) noexcept {
    return static_cast<HalfKPPiece>(pieceToHalfKP(piece));
}

[[nodiscard]] constexpr int makeFeatureIndex(Square kingSq, Piece piece, Square pieceSq, Color perspective) noexcept {
    int pieceCode = pieceToHalfKP(piece);
    if (pieceCode < 0) return -1;
    int kSq = static_cast<int>(kingSq);
    int pSq = static_cast<int>(pieceSq);
    if (perspective == Color::Black) {
        kSq ^= 56;
        pSq ^= 56;
        pieceCode = (pieceCode < 5) ? (pieceCode + 5) : (pieceCode - 5);
    }
    return (kSq * 640) + (pieceCode * 64) + pSq;
}

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using namespace Boson::eval::nnue;
}

#endif // BOSON_NNUE_TYPES_HPP
