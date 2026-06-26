#include "board/MoveExecutor.hpp"

namespace Boson {

void MoveExecutor::makeMove(Position& pos, const Move& move, UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color us = pos.getSideToMove();

    // 1. Save critical irreversible state flags into our lightweight stack container
    undoState.castlingRights = pos.getCastlingRights();
    undoState.enPassantSquare = pos.getEnPassantSquare();
    undoState.halfmoveClock = pos.getHalfmoveClock();
    undoState.capturedPiece = Piece::None;

    // Identify moving piece identity
    Piece movingPiece = Piece::None;
    for (uint8_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & Bitboards::getSquareBit(from)) {
            movingPiece = static_cast<Piece>(p);
            break;
        }
    }

    // 2. Clear moving piece from its origin square
    pos.clearPieceBit(from, movingPiece);

    // 3. Capture Processing: Scan for destination piece collisions
    for (uint8_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & Bitboards::getSquareBit(to)) {
            undoState.capturedPiece = static_cast<Piece>(p);
            pos.clearPieceBit(to, undoState.capturedPiece);
            break;
        }
    }

    // 4. Place moving piece on its destination square
    pos.setPieceBit(to, movingPiece);

    // 5. Incrementally update the composite total hardware occupancy masks
    pos.updateOccupancy();

    // 6. Complete structural metadata adjustments
    pos.setSideToMove(!us);
    
    // Clear en passant square for next ply by default (Phase A quiet rule)
    pos.setEnPassantSquare(Square::None); 

    if (movingPiece == Piece::WhitePawn || movingPiece == Piece::BlackPawn || undoState.capturedPiece != Piece::None) {
        pos.setHalfmoveClock(0);
    } else {
        pos.setHalfmoveClock(pos.getHalfmoveClock() + 1);
    }

    if (us == Color::Black) {
        pos.setFullmoveNumber(pos.getFullmoveNumber() + 1);
    }
}

void MoveExecutor::undoMove(Position& pos, const Move& move, const UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color them = pos.getSideToMove(); // Current side represents 'them' relative to the original move

    // Identify moving piece identity at destination square
    Piece movingPiece = Piece::None;
    for (uint8_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & Bitboards::getSquareBit(to)) {
            movingPiece = static_cast<Piece>(p);
            break;
        }
    }

    // Surgical state reversal sequence
    pos.clearPieceBit(to, movingPiece);
    if (undoState.capturedPiece != Piece::None) {
        pos.setPieceBit(to, undoState.capturedPiece);
    }
    pos.setPieceBit(from, movingPiece);

    // Restore cached historical flags
    pos.updateOccupancy();
    pos.setSideToMove(!them);
    pos.setCastlingRights(undoState.castlingRights);
    pos.setEnPassantSquare(undoState.enPassantSquare);
    pos.setHalfmoveClock(undoState.halfmoveClock);

    if (!them == Color::Black) {
        pos.setFullmoveNumber(pos.getFullmoveNumber() - 1);
    }
}

} // namespace Boson