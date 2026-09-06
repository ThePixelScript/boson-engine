#ifndef BOSON_POSITION_HPP
#define BOSON_POSITION_HPP

#include <array>
#include "Bitboard.hpp"
#include "Color.hpp"
#include "Piece.hpp"
#include "Castling.hpp"
#include "Square.hpp"

namespace Boson {

struct BoardSnapshot {
    std::array<Bitboard, 12> pieces{};
    std::array<Bitboard, 3>  occupancy{};
    Color sideToMove{Color::White};
    Square enPassantSquare{Square::None};
    CastlingRights castlingRights{CastlingRights::None};
    uint16_t halfmoveClock{0};
    uint16_t fullmoveNumber{1};
    Square whiteKingSquare{Square::None};
    Square blackKingSquare{Square::None};
};

struct UndoState;

class Position {
public:
    Position() noexcept;

    Bitboard getPieceBitboard(Piece piece) const noexcept { return m_pieces[static_cast<size_t>(piece)]; }
    Bitboard getColorOccupancy(Color color) const noexcept { return m_occupancy[static_cast<size_t>(color)]; }
    Bitboard getTotalOccupancy() const noexcept { return m_occupancy[static_cast<size_t>(Color::None)]; }
    Color getSideToMove() const noexcept { return m_sideToMove; }
    Color sideToMove() const noexcept { return m_sideToMove; }
    Square getEnPassantSquare() const noexcept { return m_enPassantSquare; }
    CastlingRights getCastlingRights() const noexcept { return m_castlingRights; }
    uint16_t getHalfmoveClock() const noexcept { return m_halfmoveClock; }
    uint16_t getFullmoveNumber() const noexcept { return m_fullmoveNumber; }

    void clearPieceBit(Square sq, Piece piece) noexcept;
    void setPieceBit(Square sq, Piece piece) noexcept;

    void setPiece(Square sq, Piece piece) noexcept;
    void setSideToMove(Color color) noexcept { m_sideToMove = color; }
    void setEnPassantSquare(Square sq) noexcept;
    void setCastlingRights(CastlingRights rights) noexcept;
    void setHalfmoveClock(uint16_t clock) noexcept { m_halfmoveClock = clock; }
    void setFullmoveNumber(uint16_t number) noexcept { m_fullmoveNumber = number; }

    [[nodiscard]] bool hasNonPawnMaterial(Color side) const noexcept {
        const Bitboard n = (side == Color::White) ? m_pieces[static_cast<size_t>(Piece::WhiteKnight)]
                                                  : m_pieces[static_cast<size_t>(Piece::BlackKnight)];
        const Bitboard b = (side == Color::White) ? m_pieces[static_cast<size_t>(Piece::WhiteBishop)]
                                                  : m_pieces[static_cast<size_t>(Piece::BlackBishop)];
        const Bitboard r = (side == Color::White) ? m_pieces[static_cast<size_t>(Piece::WhiteRook)]
                                                  : m_pieces[static_cast<size_t>(Piece::BlackRook)];
        const Bitboard q = (side == Color::White) ? m_pieces[static_cast<size_t>(Piece::WhiteQueen)]
                                                  : m_pieces[static_cast<size_t>(Piece::BlackQueen)];
        return (n | b | r | q) != 0ULL;
    }

    void makeNullMove(UndoState& undoState) noexcept;
    void undoNullMove(const UndoState& undoState) noexcept;
        
    void clearState() noexcept;
    void updateOccupancy() noexcept;
    void syncKingSquaresFromBitboards() noexcept;
    BoardSnapshot captureSnapshot() const noexcept;
    bool matchesSnapshot(const BoardSnapshot& snapshot) const noexcept;

    bool operator==(const Position& rhs) const noexcept {
        return m_pieces == rhs.m_pieces
            && m_occupancy == rhs.m_occupancy
            && m_sideToMove == rhs.m_sideToMove
            && m_enPassantSquare == rhs.m_enPassantSquare
            && m_castlingRights == rhs.m_castlingRights
            && m_halfmoveClock == rhs.m_halfmoveClock
            && m_fullmoveNumber == rhs.m_fullmoveNumber
            && m_whiteKingSquare == rhs.m_whiteKingSquare
            && m_blackKingSquare == rhs.m_blackKingSquare
            && m_hashKey == rhs.m_hashKey;
    }

    uint64_t getHashKey() const noexcept { return m_hashKey; }

    void togglePieceHash(Square sq, Piece piece) noexcept;
    void toggleSideHash() noexcept;

    Square getKingSquare(Color side) const noexcept {
        return (side == Color::White) ? m_whiteKingSquare : m_blackKingSquare;
    }
    Square kingSquare(Color side) const noexcept {
        return getKingSquare(side);
    }
    void setKingSquare(Color color, Square sq) noexcept;

private:
    Square m_whiteKingSquare;
    Square m_blackKingSquare;
    std::array<Bitboard, 12> m_pieces;
    std::array<Bitboard, 3>  m_occupancy;
    Color m_sideToMove;
    Square m_enPassantSquare;
    CastlingRights m_castlingRights;
    uint16_t m_halfmoveClock;
    uint16_t m_fullmoveNumber;
    uint64_t m_hashKey = 0ULL;
};

} // namespace Boson

#endif // BOSON_POSITION_HPP