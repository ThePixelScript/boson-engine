#include "eval/nnue/FeatureTransformer.hpp"
#include "board/Bitboard.hpp"
#include <algorithm>
#include <bit>

namespace Boson::eval::nnue {

std::vector<int> FeatureTransformer::getActiveFeatures(const Position& pos, Color perspective) {
    std::vector<int> features;
    features.reserve(32);

    const Square kingSq = pos.getKingSquare(perspective);

    for (uint8_t p = 0; p < 12; ++p) {
        const Piece piece = static_cast<Piece>(p);
        if (piece == Piece::WhiteKing || piece == Piece::BlackKing) {
            continue;
        }

        Bitboard bb = pos.getPieceBitboard(piece);
        while (bb) {
            int sq = std::countr_zero(bb);
            int idx = makeFeatureIndex(kingSq, piece, static_cast<Square>(sq), perspective);
            features.push_back(idx);
            bb &= bb - 1;
        }
    }

    std::sort(features.begin(), features.end());
    return features;
}

void FeatureTransformer::computeDeltas(const Position& before,
                                       const Position& after,
                                       const Move& move,
                                       Color perspective,
                                       std::vector<int>& removed,
                                       std::vector<int>& added) {
    removed.clear();
    added.clear();

    const Square kingBefore = before.getKingSquare(perspective);
    const Square kingAfter = after.getKingSquare(perspective);

    // If king of this perspective moved, full feature refresh is required
    if (kingBefore != kingAfter) {
        removed = getActiveFeatures(before, perspective);
        added = getActiveFeatures(after, perspective);
        return;
    }

    const Square kingSq = kingBefore;
    const Square from = move.getFromSquare();
    const Square to = move.getToSquare();
    const Color moverSide = before.getSideToMove();

    // Identify moving piece from the before position
    Piece movingPiece = Piece::None;
    const Bitboard fromBit = 1ULL << static_cast<size_t>(from);
    for (uint8_t p = 0; p < 12; ++p) {
        if (before.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
            movingPiece = static_cast<Piece>(p);
            break;
        }
    }

    // Opponent king move / castling (our king did not move)
    if (movingPiece == Piece::WhiteKing || movingPiece == Piece::BlackKing) {
        if (move.isCastling()) {
            Square rookFrom = Square::None;
            Square rookTo = Square::None;
            Piece rookPiece = (moverSide == Color::White) ? Piece::WhiteRook : Piece::BlackRook;

            if (to == Square::G1) { rookFrom = Square::H1; rookTo = Square::F1; }
            else if (to == Square::C1) { rookFrom = Square::A1; rookTo = Square::D1; }
            else if (to == Square::G8) { rookFrom = Square::H8; rookTo = Square::F8; }
            else if (to == Square::C8) { rookFrom = Square::A8; rookTo = Square::D8; }

            if (rookFrom != Square::None) {
                removed.push_back(makeFeatureIndex(kingSq, rookPiece, rookFrom, perspective));
                added.push_back(makeFeatureIndex(kingSq, rookPiece, rookTo, perspective));
            }
        } else {
            const Bitboard toBit = 1ULL << static_cast<size_t>(to);
            for (uint8_t p = 0; p < 12; ++p) {
                if (before.getPieceBitboard(static_cast<Piece>(p)) & toBit) {
                    Piece captured = static_cast<Piece>(p);
                    if (captured != Piece::WhiteKing && captured != Piece::BlackKing) {
                        removed.push_back(makeFeatureIndex(kingSq, captured, to, perspective));
                    }
                    break;
                }
            }
        }
        std::sort(removed.begin(), removed.end());
        std::sort(added.begin(), added.end());
        return;
    }

    // Non-king moving piece removed from 'from'
    removed.push_back(makeFeatureIndex(kingSq, movingPiece, from, perspective));

    // Handle captures
    if (move.isEnPassant()) {
        const Square victimSq = (moverSide == Color::White)
            ? static_cast<Square>(static_cast<int>(to) - 8)
            : static_cast<Square>(static_cast<int>(to) + 8);
        const Piece victimPiece = (moverSide == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        removed.push_back(makeFeatureIndex(kingSq, victimPiece, victimSq, perspective));
    } else {
        const Bitboard toBit = 1ULL << static_cast<size_t>(to);
        for (uint8_t p = 0; p < 12; ++p) {
            if (before.getPieceBitboard(static_cast<Piece>(p)) & toBit) {
                Piece captured = static_cast<Piece>(p);
                removed.push_back(makeFeatureIndex(kingSq, captured, to, perspective));
                break;
            }
        }
    }

    // Handle placement on 'to' (with promotion handling)
    if (move.isPromotion()) {
        Piece promoPiece = Piece::None;
        const auto promoType = move.getPromotionPiece();
        if (moverSide == Color::White) {
            switch (promoType) {
                case Move::PromotionPiece::Queen:  promoPiece = Piece::WhiteQueen; break;
                case Move::PromotionPiece::Rook:   promoPiece = Piece::WhiteRook; break;
                case Move::PromotionPiece::Bishop: promoPiece = Piece::WhiteBishop; break;
                case Move::PromotionPiece::Knight: promoPiece = Piece::WhiteKnight; break;
                default: promoPiece = Piece::WhiteQueen; break;
            }
        } else {
            switch (promoType) {
                case Move::PromotionPiece::Queen:  promoPiece = Piece::BlackQueen; break;
                case Move::PromotionPiece::Rook:   promoPiece = Piece::BlackRook; break;
                case Move::PromotionPiece::Bishop: promoPiece = Piece::BlackBishop; break;
                case Move::PromotionPiece::Knight: promoPiece = Piece::BlackKnight; break;
                default: promoPiece = Piece::BlackQueen; break;
            }
        }
        added.push_back(makeFeatureIndex(kingSq, promoPiece, to, perspective));
    } else {
        added.push_back(makeFeatureIndex(kingSq, movingPiece, to, perspective));
    }

    std::sort(removed.begin(), removed.end());
    std::sort(added.begin(), added.end());
}

} // namespace Boson::eval::nnue
