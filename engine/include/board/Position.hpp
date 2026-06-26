#ifndef BOSON_POSITION_HPP
#define BOSON_POSITION_HPP

#include <array>
#include "Bitboard.hpp"
#include "Color.hpp"
#include "Piece.hpp"
#include "Castling.hpp"
#include "Square.hpp"

namespace Boson {

class Position {
public:
    Position() noexcept;

    // Core read-only diagnostic metrics
    Bitboard getPieceBitboard(Piece piece) const noexcept { return m_pieces[static_cast<size_t>(piece)]; }
    Bitboard getColorOccupancy(Color color) const noexcept { return m_occupancy[static_cast<size_t>(color)]; }
    Bitboard getTotalOccupancy() const noexcept { return m_occupancy[static_cast<size_t>(Color::None)]; }
    Color getSideToMove() const noexcept { return m_sideToMove; }
    Square getEnPassantSquare() const noexcept { return m_enPassantSquare; }
    CastlingRights getCastlingRights() const noexcept { return m_castlingRights; }
    uint16_t getHalfmoveClock() const noexcept { return m_halfmoveClock; }
    uint16_t getFullmoveNumber() const noexcept { return m_fullmoveNumber; }

    // Surgical mutators for direct high-velocity bit updates
    void clearPieceBit(Square sq, Piece piece) noexcept { m_pieces[static_cast<size_t>(piece)] &= ~Bitboards::getSquareBit(sq); }
    void setPieceBit(Square sq, Piece piece) noexcept { m_pieces[static_cast<size_t>(piece)] |= Bitboards::getSquareBit(sq); }

    // Mutators strictly for the builder/parsing pipeline
    void setPiece(Square sq, Piece piece) noexcept;
    void setSideToMove(Color color) noexcept { m_sideToMove = color; }
    void setEnPassantSquare(Square sq) noexcept { m_enPassantSquare = sq; }
    void setCastlingRights(CastlingRights rights) noexcept { m_castlingRights = rights; }
    void setHalfmoveClock(uint16_t clock) noexcept { m_halfmoveClock = clock; }
    void setFullmoveNumber(uint16_t number) noexcept { m_fullmoveNumber = number; }
    
    void clearState() noexcept;
    void updateOccupancy() noexcept;
    void debugPrintToConsole() const noexcept;

private:
    std::array<Bitboard, 12> m_pieces;
    std::array<Bitboard, 3>  m_occupancy; // [0] White, [1] Black, [2] Combined Total
    Color m_sideToMove;
    Square m_enPassantSquare;
    CastlingRights m_castlingRights;
    uint16_t m_halfmoveClock;
    uint16_t m_fullmoveNumber;
    uint64_t m_zobristKey; 
};

} // namespace Boson

#endif // BOSON_POSITION_HPP