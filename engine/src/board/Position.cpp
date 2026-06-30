#include "board/Position.hpp"
#include "search/Zobrist.hpp"
#include <iostream>
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
    togglePieceHash(sq, piece); // Maintain parsing/builder updates dynamically
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

    // Use integer indices for the loop, then cast to Piece
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

void Position::debugPrintToConsole() const noexcept {
    std::cout << "\n +---+---+---+---+---+---+---+──+\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << " " << (rank + 1) << " |";
        for (int file = 0; file < 8; ++file) {
            Square currentSq = static_cast<Square>(rank * 8 + file);
            Bitboard currentMask = Bitboards::getSquareBit(currentSq);
            char activePieceSymbol = '.';

            if (m_pieces[static_cast<size_t>(Piece::WhitePawn)] & currentMask) activePieceSymbol = 'P';
            else if (m_pieces[static_cast<size_t>(Piece::WhiteKnight)] & currentMask) activePieceSymbol = 'N';
            else if (m_pieces[static_cast<size_t>(Piece::WhiteBishop)] & currentMask) activePieceSymbol = 'B';
            else if (m_pieces[static_cast<size_t>(Piece::WhiteRook)] & currentMask) activePieceSymbol = 'R';
            else if (m_pieces[static_cast<size_t>(Piece::WhiteQueen)] & currentMask) activePieceSymbol = 'Q';
            else if (m_pieces[static_cast<size_t>(Piece::WhiteKing)] & currentMask) activePieceSymbol = 'K';
            else if (m_pieces[static_cast<size_t>(Piece::BlackPawn)] & currentMask) activePieceSymbol = 'p';
            else if (m_pieces[static_cast<size_t>(Piece::BlackKnight)] & currentMask) activePieceSymbol = 'n';
            else if (m_pieces[static_cast<size_t>(Piece::BlackBishop)] & currentMask) activePieceSymbol = 'b';
            else if (m_pieces[static_cast<size_t>(Piece::BlackRook)] & currentMask) activePieceSymbol = 'r';
            else if (m_pieces[static_cast<size_t>(Piece::BlackQueen)] & currentMask) activePieceSymbol = 'q';
            else if (m_pieces[static_cast<size_t>(Piece::BlackKing)] & currentMask) activePieceSymbol = 'k';

            std::cout << " " << activePieceSymbol << " |";
        }
        std::cout << "\n +---+---+---+---+---+---+---+──+\n";
    }
    std::cout << "    a   b   c   d   e   f   g   h\n\n";
    std::cout << "Side to move: " << (m_sideToMove == Color::White ? "White" : "Black") << "\n";
}

void Position::setKingSquare(Color color, Square sq) noexcept {
    if (color == Color::White) m_whiteKingSquare = sq;
    else m_blackKingSquare = sq;
}

} // namespace Boson