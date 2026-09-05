#include "board/MoveExecutor.hpp"
#include <cassert>

namespace Boson {

namespace {

Piece promotionPieceFromMove(Color us, Move::PromotionPiece promoType) noexcept {
    if (us == Color::White) {
        switch (promoType) {
            case Move::PromotionPiece::Queen:  return Piece::WhiteQueen;
            case Move::PromotionPiece::Rook:   return Piece::WhiteRook;
            case Move::PromotionPiece::Bishop: return Piece::WhiteBishop;
            case Move::PromotionPiece::Knight: return Piece::WhiteKnight;
            default: return Piece::WhiteQueen;
        }
    }
    switch (promoType) {
        case Move::PromotionPiece::Queen:  return Piece::BlackQueen;
        case Move::PromotionPiece::Rook:   return Piece::BlackRook;
        case Move::PromotionPiece::Bishop: return Piece::BlackBishop;
        case Move::PromotionPiece::Knight: return Piece::BlackKnight;
        default: return Piece::BlackQueen;
    }
}

struct CastlingRookMove {
    Square from{Square::None};
    Square to{Square::None};
    Piece piece{Piece::None};
};

CastlingRookMove castlingRookMoveFor(Square kingTo, Color us) noexcept {
    const Piece rookPiece = (us == Color::White) ? Piece::WhiteRook : Piece::BlackRook;
    if (kingTo == Square::G1) return {Square::H1, Square::F1, rookPiece};
    if (kingTo == Square::C1) return {Square::A1, Square::D1, rookPiece};
    if (kingTo == Square::G8) return {Square::H8, Square::F8, rookPiece};
    if (kingTo == Square::C8) return {Square::A8, Square::D8, rookPiece};
    return {};
}

void relocateCastlingRook(Position& pos, const CastlingRookMove& rookMove) noexcept {
    if (rookMove.from == Square::None || rookMove.piece == Piece::None) return;
    pos.clearPieceBit(rookMove.from, rookMove.piece);
    pos.setPieceBit(rookMove.to, rookMove.piece);
}

} // namespace

void MoveExecutor::makeMove(Position& pos, const Move& move, UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color us = pos.getSideToMove();
    const Color them = (us == Color::White) ? Color::Black : Color::White;

    const Bitboard fromBit = 1ULL << static_cast<size_t>(from);
    const Bitboard toBit = 1ULL << static_cast<size_t>(to);

    // Step 1: Save UndoState snapshot
    undoState.castlingRights = pos.getCastlingRights();
    undoState.enPassantSquare = pos.getEnPassantSquare();
    undoState.halfmoveClock = pos.getHalfmoveClock();
    undoState.capturedPiece = Piece::None;
    undoState.castlingRookFrom = Square::None;
    undoState.castlingRookTo = Square::None;
    undoState.castlingRookPiece = Piece::None;
    undoState.hashKey = pos.getHashKey();

    // Step 2: Remove moving piece from source square
    Piece movingPiece = Piece::None;
    for (uint8_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
            movingPiece = static_cast<Piece>(p);
            break;
        }
    }
    undoState.movingPiece = movingPiece;
    pos.clearPieceBit(from, movingPiece);

    // Step 3: Remove captured piece
    if (move.isEnPassant()) {
        Square victimSq = (us == Color::White)
            ? static_cast<Square>(static_cast<int>(to) - 8)
            : static_cast<Square>(static_cast<int>(to) + 8);
        undoState.capturedPiece = (us == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        pos.clearPieceBit(victimSq, undoState.capturedPiece);
    } else {
        for (uint8_t p = 0; p < 12; ++p) {
            if (pos.getPieceBitboard(static_cast<Piece>(p)) & toBit) {
                undoState.capturedPiece = static_cast<Piece>(p);
                pos.clearPieceBit(to, undoState.capturedPiece);
                break;
            }
        }
    }

    // Step 4: Place moving piece on destination square
    if (move.isPromotion()) {
        auto promoType = move.getPromotionPiece();
        Piece promoPiece = promotionPieceFromMove(us, promoType);
        pos.setPieceBit(to, promoPiece);
    } else {
        pos.setPieceBit(to, movingPiece);
    }

    if (move.isCastling()) {
        const CastlingRookMove rookMove = castlingRookMoveFor(to, us);
        undoState.castlingRookFrom = rookMove.from;
        undoState.castlingRookTo = rookMove.to;
        undoState.castlingRookPiece = rookMove.piece;
        relocateCastlingRook(pos, rookMove);
    }

    // Step 5: Update occupancies & king square caches
    pos.updateOccupancy();

    // Step 6: Update game state & clocks
    uint8_t rightsRaw = static_cast<uint8_t>(pos.getCastlingRights());

    if (movingPiece == Piece::WhiteKing) {
        rightsRaw &= ~(static_cast<uint8_t>(CastlingRights::WhiteOO) | static_cast<uint8_t>(CastlingRights::WhiteOOO));
    } else if (movingPiece == Piece::BlackKing) {
        rightsRaw &= ~(static_cast<uint8_t>(CastlingRights::BlackOO) | static_cast<uint8_t>(CastlingRights::BlackOOO));
    }

    if (from == Square::H1 || to == Square::H1) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::WhiteOO);
    if (from == Square::A1 || to == Square::A1) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::WhiteOOO);
    if (from == Square::H8 || to == Square::H8) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::BlackOO);
    if (from == Square::A8 || to == Square::A8) rightsRaw &= ~static_cast<uint8_t>(CastlingRights::BlackOOO);

    pos.setCastlingRights(static_cast<CastlingRights>(rightsRaw));

    if (move.isDoublePawnPush()) {
        Square epTarget = (us == Color::White) ? static_cast<Square>(static_cast<int>(from) + 8)
                                               : static_cast<Square>(static_cast<int>(from) - 8);
        pos.setEnPassantSquare(epTarget);
    } else {
        pos.setEnPassantSquare(Square::None);
    }

    const bool isPawnMove = (movingPiece == Piece::WhitePawn || movingPiece == Piece::BlackPawn);
    const bool isCapture = (undoState.capturedPiece != Piece::None);
    if (isPawnMove || isCapture) {
        pos.setHalfmoveClock(0);
    } else {
        pos.setHalfmoveClock(static_cast<uint16_t>(pos.getHalfmoveClock() + 1));
    }

    if (us == Color::Black) {
        pos.setFullmoveNumber(pos.getFullmoveNumber() + 1);
    }

    // Step 7: Switch side to move
    pos.setSideToMove(them);
    pos.toggleSideHash();

    // Position invariant assertions (compiled out in Release builds via NDEBUG)
    assert((pos.getColorOccupancy(Color::White) | pos.getColorOccupancy(Color::Black)) == pos.getTotalOccupancy());
    assert((pos.getColorOccupancy(Color::White) & pos.getColorOccupancy(Color::Black)) == 0ULL);
}

void MoveExecutor::undoMove(Position& pos, const Move& move, const UndoState& undoState) noexcept {
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color currentSide = pos.getSideToMove();
    const Color originalUs = (currentSide == Color::White) ? Color::Black : Color::White;

    // Step 1: Revert side to move & fullmove number
    if (originalUs == Color::Black) {
        pos.setFullmoveNumber(pos.getFullmoveNumber() - 1);
    }

    pos.setSideToMove(originalUs);
    pos.toggleSideHash(); 

    // Step 2: Revert piece placement
    if (move.isCastling()) {
        pos.clearPieceBit(undoState.castlingRookTo, undoState.castlingRookPiece);
        pos.setPieceBit(undoState.castlingRookFrom, undoState.castlingRookPiece);
    }

    if (move.isPromotion()) {
        Piece promoPiece = promotionPieceFromMove(originalUs, move.getPromotionPiece());
        pos.clearPieceBit(to, promoPiece);
    } else {
        pos.clearPieceBit(to, undoState.movingPiece);
    }

    pos.setPieceBit(from, undoState.movingPiece);

    // Step 3: Revert captured piece
    if (move.isEnPassant()) {
        Square victimSq = static_cast<Square>((originalUs == Color::White) ? (static_cast<int>(to) - 8) : (static_cast<int>(to) + 8));
        pos.setPieceBit(victimSq, undoState.capturedPiece);
    } else if (undoState.capturedPiece != Piece::None) {
        pos.setPieceBit(to, undoState.capturedPiece);
    }

    // Step 4: Restore game state & clocks
    pos.setCastlingRights(undoState.castlingRights);
    pos.setEnPassantSquare(undoState.enPassantSquare);
    pos.setHalfmoveClock(static_cast<uint16_t>(undoState.halfmoveClock));

    // Step 5: Update occupancies & king square caches
    pos.updateOccupancy();

    // Verification: Hash key symmetry and position invariants
    assert(pos.getHashKey() == undoState.hashKey && "Hash key desynchronized after undoMove");
    assert((pos.getColorOccupancy(Color::White) | pos.getColorOccupancy(Color::Black)) == pos.getTotalOccupancy());
    assert((pos.getColorOccupancy(Color::White) & pos.getColorOccupancy(Color::Black)) == 0ULL);
}

void MoveExecutor::makeNullMove(Position& pos, UndoState& undoState) noexcept {
    pos.makeNullMove(undoState);
}

void MoveExecutor::undoNullMove(Position& pos, const UndoState& undoState) noexcept {
    pos.undoNullMove(undoState);
}

} // namespace Boson