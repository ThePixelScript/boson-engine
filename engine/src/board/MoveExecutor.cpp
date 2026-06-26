#include "board/MoveExecutor.hpp"

namespace Boson {

void MoveExecutor::makeMove(Position& pos, const Move& move, UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color us = pos.getSideToMove();
    const Color them = !us;

    // 1. Save state flags into UndoState container
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

    // 2. Clear moving piece from source
    pos.clearPieceBit(from, movingPiece);

    // 3. Process Capture Collisions (Normal, Promotion, or En Passant)
    if (move.isEnPassant()) {
        Square victimSq = (us == Color::White) ? static_cast<Square>(static_cast<int>(to) - 8) 
                                               : static_cast<Square>(static_cast<int>(to) + 8);
        undoState.capturedPiece = (us == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        pos.clearPieceBit(victimSq, undoState.capturedPiece);
    } else {
        for (uint8_t p = 0; p < 12; ++p) {
            if (pos.getPieceBitboard(static_cast<Piece>(p)) & Bitboards::getSquareBit(to)) {
                undoState.capturedPiece = static_cast<Piece>(p);
                pos.clearPieceBit(to, undoState.capturedPiece);
                break;
            }
        }
    }

    // 4. Place Moving or Promoted Piece
    if (move.isPromotion()) {
        auto promoType = move.getPromotionPiece();
        Piece promoPiece = Piece::None;
        if (us == Color::White) {
            if (promoType == Move::PromotionPiece::Queen)  promoPiece = Piece::WhiteQueen;
            if (promoType == Move::PromotionPiece::Rook)   promoPiece = Piece::WhiteRook;
            if (promoType == Move::PromotionPiece::Bishop) promoPiece = Piece::WhiteBishop;
            if (promoType == Move::PromotionPiece::Knight) promoPiece = Piece::WhiteKnight;
        } else {
            if (promoType == Move::PromotionPiece::Queen)  promoPiece = Piece::BlackQueen;
            if (promoType == Move::PromotionPiece::Rook)   promoPiece = Piece::BlackRook;
            if (promoType == Move::PromotionPiece::Bishop) promoPiece = Piece::BlackBishop;
            if (promoType == Move::PromotionPiece::Knight) promoPiece = Piece::BlackKnight;
        }
        pos.setPieceBit(to, promoPiece);
    } else {
        pos.setPieceBit(to, movingPiece);
    }

    // 5. Castling Secondary Rook Relocation
    if (move.isCastling()) {
        if (to == Square::G1) { // White Kingside
            pos.clearPieceBit(Square::H1, Piece::WhiteRook);
            pos.setPieceBit(Square::F1, Piece::WhiteRook);
        } else if (to == Square::C1) { // White Queenside
            pos.clearPieceBit(Square::A1, Piece::WhiteRook);
            pos.setPieceBit(Square::D1, Piece::WhiteRook);
        } else if (to == Square::G8) { // Black Kingside
            pos.clearPieceBit(Square::H8, Piece::BlackRook);
            pos.setPieceBit(Square::F8, Piece::BlackRook);
        } else if (to == Square::C8) { // Black Queenside
            pos.clearPieceBit(Square::A8, Piece::BlackRook);
            pos.setPieceBit(Square::D8, Piece::BlackRook);
        }
    }

    // 6. Castling Rights Revocation Math (Safely cast for bitwise operations)
    uint8_t rightsRaw = static_cast<uint8_t>(pos.getCastlingRights());
    
    if (movingPiece == Piece::WhiteKing) {
        rightsRaw &= ~(static_cast<uint8_t>(CastlingRights::WhiteOO) | static_cast<uint8_t>(CastlingRights::WhiteOOO));
    } else if (movingPiece == Piece::BlackKing) {
        rightsRaw &= ~(static_cast<uint8_t>(CastlingRights::BlackOO) | static_cast<uint8_t>(CastlingRights::BlackOOO));
    }
    
    // Revoke if rooks move or get captured directly
    if (from == Square::H1 || to == Square::H1) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::WhiteOO);
    if (from == Square::A1 || to == Square::A1) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::WhiteOOO);
    if (from == Square::H8 || to == Square::H8) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::BlackOO);
    if (from == Square::A8 || to == Square::A8) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::BlackOOO);
    
    pos.setCastlingRights(static_cast<CastlingRights>(rightsRaw));

    // 7. Handle Transient En Passant Target Mapping
    if (move.isDoublePawnPush()) {
        Square epTarget = (us == Color::White) ? static_cast<Square>(static_cast<int>(from) + 8)
                                               : static_cast<Square>(static_cast<int>(from) - 8);
        pos.setEnPassantSquare(epTarget);
    } else {
        pos.setEnPassantSquare(Square::None);
    }

    // Incremental Rebuild of Occupancy Layers
    pos.updateOccupancy();
    pos.setSideToMove(them);

    // Update Clocks
    if (movingPiece == Piece::WhitePawn || movingPiece == Piece::BlackPawn || undoState.capturedPiece != Piece::None) {
        pos.setHalfmoveClock(0);
    } else {
        pos.setHalfmoveClock(pos.getHalfmoveClock() + 1);
    }
    if (us == Color::Black) pos.setFullmoveNumber(pos.getFullmoveNumber() + 1);
}

void MoveExecutor::undoMove(Position& pos, const Move& move, const UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color them = pos.getSideToMove();
    const Color us = !them;

    // Clear whatever is currently on the destination square
    Piece pieceOnTo = Piece::None;
    for (uint8_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & Bitboards::getSquareBit(to)) {
            pieceOnTo = static_cast<Piece>(p);
            break;
        }
    }
    pos.clearPieceBit(to, pieceOnTo);

    // Restore Moving Piece back to source
    if (move.isPromotion()) {
        Piece originalPawn = (us == Color::White) ? Piece::WhitePawn : Piece::BlackPawn;
        pos.setPieceBit(from, originalPawn);
    } else {
        pos.setPieceBit(from, pieceOnTo);
    }

    // Restore Captured Pieces
    if (move.isEnPassant()) {
        Square victimSq = (us == Color::White) ? static_cast<Square>(static_cast<int>(to) - 8) 
                                               : static_cast<Square>(static_cast<int>(to) + 8);
        pos.setPieceBit(victimSq, undoState.capturedPiece);
    } else if (undoState.capturedPiece != Piece::None) {
        pos.setPieceBit(to, undoState.capturedPiece);
    }

    // Invert Castling Rook Placement
    if (move.isCastling()) {
        if (to == Square::G1) {
            pos.clearPieceBit(Square::F1, Piece::WhiteRook);
            pos.setPieceBit(Square::H1, Piece::WhiteRook);
        } else if (to == Square::C1) {
            pos.clearPieceBit(Square::D1, Piece::WhiteRook);
            pos.setPieceBit(Square::A1, Piece::WhiteRook);
        } else if (to == Square::G8) {
            pos.clearPieceBit(Square::F8, Piece::BlackRook);
            pos.setPieceBit(Square::H8, Piece::BlackRook);
        } else if (to == Square::C8) {
            pos.clearPieceBit(Square::D8, Piece::BlackRook);
            pos.setPieceBit(Square::A8, Piece::BlackRook);
        }
    }

    // Revert Transient Values and Flags
    pos.setCastlingRights(undoState.castlingRights);
    pos.setEnPassantSquare(undoState.enPassantSquare);
    pos.setHalfmoveClock(undoState.halfmoveClock);
    pos.setSideToMove(us);

    if (us == Color::Black) pos.setFullmoveNumber(pos.getFullmoveNumber() - 1);
    pos.updateOccupancy();
}

} // namespace Boson