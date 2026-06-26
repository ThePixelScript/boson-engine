#include "board/Position.hpp"
#include <iostream>

namespace Boson {

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
m_zobristKey = 0ULL;
}

void Position::debugPrintToConsole() const noexcept {
// Structural diagnostic string output for verification checks
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
std::cout << " a b c d e f g h\n\n";
std::cout << "Side to move: " << (m_sideToMove == Color::White ? "White" : "Black") << "\n";
}

} // namespace Boson

void Position::setPiece(Square sq, Piece piece) noexcept {
    if (sq == Square::None || piece == Piece::None) return;
    m_pieces[static_cast<size_t>(piece)] |= Bitboards::getSquareBit(sq);
}

void Position::updateOccupancy() noexcept {
    m_occupancy[static_cast<size_t>(Color::White)] = Bitboards::Empty;
    m_occupancy[static_cast<size_t>(Color::Black)] = Bitboards::Empty;

    for (size_t p = 0; p < 6; ++p) {
        m_occupancy[static_cast<size_t>(Color::White)] |= m_pieces[p];
    }
    for (size_t p = 6; p < 12; ++p) {
        m_occupancy[static_cast<size_t>(Color::Black)] |= m_pieces[p];
    }
    m_occupancy[static_cast<size_t>(Color::None)] = 
        m_occupancy[static_cast<size_t>(Color::White)] | m_occupancy[static_cast<size_t>(Color::Black)];
}