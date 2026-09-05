#include "board/Position.hpp"
#include "board/UndoState.hpp"
#include "search/Zobrist.hpp"
#include <iostream>
#include <cassert>
#include <intrin.h>

namespace Boson {

namespace {

Square bitboardToSquare(Bitboard bb) noexcept {
    if (!bb) return Square::None;
    unsigned long index = 0;
    _BitScanForward64(&index, bb);
    return static_cast<Square>(index);
}

} // namespace

Position::Position() noexcept {
    clearState();
}

void Position::clearState() noexcept {
    Zobrist::initialize();
    m_pieces.fill(Bitboards::Empty);
    m_occupancy.fill(Bitboards::Empty);
    m_sideToMove = Color::White;
    m_enPassantSquare = Square::None;
    m_castlingRights = CastlingRights::None;
    m_halfmoveClock = 0;
    m_fullmoveNumber = 1;
    m_hashKey = 0ULL;
    m_whiteKingSquare = Square::None;
    m_blackKingSquare = Square::None;
}

void Position::clearPieceBit(Square sq, Piece piece) noexcept {
    m_pieces[static_cast<size_t>(piece)] &= ~Bitboards::getSquareBit(sq);
    togglePieceHash(sq, piece);
    if (piece == Piece::WhiteKing && m_whiteKingSquare == sq) {
        m_whiteKingSquare = Square::None;
    } else if (piece == Piece::BlackKing && m_blackKingSquare == sq) {
        m_blackKingSquare = Square::None;
    }
}

void Position::setPieceBit(Square sq, Piece piece) noexcept {
    m_pieces[static_cast<size_t>(piece)] |= Bitboards::getSquareBit(sq);
    togglePieceHash(sq, piece);
    if (piece == Piece::WhiteKing) {
        m_whiteKingSquare = sq;
    } else if (piece == Piece::BlackKing) {
        m_blackKingSquare = sq;
    }
}

void Position::setPiece(Square sq, Piece piece) noexcept {
    if (sq == Square::None || piece == Piece::None) return;
    m_pieces[static_cast<size_t>(piece)] |= Bitboards::getSquareBit(sq);
    togglePieceHash(sq, piece);
}

void Position::togglePieceHash(Square sq, Piece piece) noexcept {
    m_hashKey ^= Zobrist::s_pieces[static_cast<size_t>(piece)][static_cast<size_t>(sq)];
}

void Position::toggleSideHash() noexcept {
    m_hashKey ^= Zobrist::s_sideToMove;
}

void Position::updateOccupancy() noexcept {
    m_occupancy[static_cast<size_t>(Color::White)] = 0ULL;
    m_occupancy[static_cast<size_t>(Color::Black)] = 0ULL;

    for (size_t p = static_cast<size_t>(Piece::WhitePawn); p <= static_cast<size_t>(Piece::WhiteKing); ++p) {
        m_occupancy[static_cast<size_t>(Color::White)] |= m_pieces[p];
    }
    for (size_t p = static_cast<size_t>(Piece::BlackPawn); p <= static_cast<size_t>(Piece::BlackKing); ++p) {
        m_occupancy[static_cast<size_t>(Color::Black)] |= m_pieces[p];
    }

    m_occupancy[static_cast<size_t>(Color::None)] = 
        m_occupancy[static_cast<size_t>(Color::White)] | 
        m_occupancy[static_cast<size_t>(Color::Black)];
    syncKingSquaresFromBitboards();
}

void Position::syncKingSquaresFromBitboards() noexcept {
    m_whiteKingSquare = bitboardToSquare(m_pieces[static_cast<size_t>(Piece::WhiteKing)]);
    m_blackKingSquare = bitboardToSquare(m_pieces[static_cast<size_t>(Piece::BlackKing)]);
}

BoardSnapshot Position::captureSnapshot() const noexcept {
    BoardSnapshot snapshot;
    snapshot.pieces = m_pieces;
    snapshot.occupancy = m_occupancy;
    snapshot.sideToMove = m_sideToMove;
    snapshot.enPassantSquare = m_enPassantSquare;
    snapshot.castlingRights = m_castlingRights;
    snapshot.halfmoveClock = m_halfmoveClock;
    snapshot.fullmoveNumber = m_fullmoveNumber;
    snapshot.whiteKingSquare = m_whiteKingSquare;
    snapshot.blackKingSquare = m_blackKingSquare;
    return snapshot;
}

bool Position::matchesSnapshot(const BoardSnapshot& snapshot) const noexcept {
    return m_pieces == snapshot.pieces
        && m_occupancy == snapshot.occupancy
        && m_sideToMove == snapshot.sideToMove
        && m_enPassantSquare == snapshot.enPassantSquare
        && m_castlingRights == snapshot.castlingRights
        && m_halfmoveClock == snapshot.halfmoveClock
        && m_fullmoveNumber == snapshot.fullmoveNumber
        && m_whiteKingSquare == snapshot.whiteKingSquare
        && m_blackKingSquare == snapshot.blackKingSquare;
}

void Position::setKingSquare(Color color, Square sq) noexcept {
    if (color == Color::White) m_whiteKingSquare = sq;
    else m_blackKingSquare = sq;
}

void Position::setEnPassantSquare(Square sq) noexcept {
    if (m_enPassantSquare != Square::None) {
        m_hashKey ^= Zobrist::s_enPassant[static_cast<size_t>(m_enPassantSquare) % 8];
    }
    m_enPassantSquare = sq;
    if (m_enPassantSquare != Square::None) {
        m_hashKey ^= Zobrist::s_enPassant[static_cast<size_t>(m_enPassantSquare) % 8];
    }
}

void Position::setCastlingRights(CastlingRights rights) noexcept {
    m_hashKey ^= Zobrist::s_castling[static_cast<size_t>(m_castlingRights)];
    m_castlingRights = rights;
    m_hashKey ^= Zobrist::s_castling[static_cast<size_t>(m_castlingRights)];
}

void Position::makeNullMove(UndoState& undoState) noexcept {
    undoState.castlingRights = m_castlingRights;
    undoState.enPassantSquare = m_enPassantSquare;
    undoState.halfmoveClock = m_halfmoveClock;
    undoState.capturedPiece = Piece::None;
    undoState.movingPiece = Piece::None;
    undoState.hashKey = m_hashKey;

    // Clear en passant square (and XOR out its hash key if set)
    setEnPassantSquare(Square::None);

    // Switch side to move and toggle side hash
    m_sideToMove = (m_sideToMove == Color::White) ? Color::Black : Color::White;
    toggleSideHash();

    m_halfmoveClock++;
    if (m_sideToMove == Color::White) {
        m_fullmoveNumber++;
    }
}

void Position::undoNullMove(const UndoState& undoState) noexcept {
    if (m_sideToMove == Color::White) {
        m_fullmoveNumber--;
    }

    m_sideToMove = (m_sideToMove == Color::White) ? Color::Black : Color::White;
    toggleSideHash();

    setEnPassantSquare(undoState.enPassantSquare);
    m_halfmoveClock = static_cast<uint16_t>(undoState.halfmoveClock);

    assert(m_hashKey == undoState.hashKey && "Hash key desynchronized after undoNullMove");
}

} // namespace Boson