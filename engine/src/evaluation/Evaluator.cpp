#include "evaluation/Evaluator.hpp"
#include "evaluation/PieceSquareTables.hpp"
#include "search/SearchController.hpp"
#include <bit>

namespace Boson {

int Evaluator::evaluate(const Position& pos) noexcept {
    int whiteScore = 0;
    int blackScore = 0;

    // Helper lambda to accumulate material and PST positional values
    auto scorePieceType = [](Bitboard bb, int materialValue, const std::array<int16_t, 64>& pst, bool isBlack) noexcept -> int {
        int total = 0;
        while (bb) {
            unsigned long sq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&sq, bb);
            #else
                sq = __builtin_ctzll(bb);
            #endif

            total += materialValue;
            
            // Mirror rank indexing via bitwise XOR for Black: sq ^ 56
            size_t pstIndex = isBlack ? (static_cast<size_t>(sq) ^ 56) : static_cast<size_t>(sq);
            total += pst[pstIndex];

            bb &= bb - 1; // Pop LS1B
        }
        return total;
    };

    // 1. Accumulate White Assets
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhitePawn),   PAWN_VALUE,   PieceSquareTables::Pawn,   false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteKnight), KNIGHT_VALUE, PieceSquareTables::Knight, false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteBishop), BISHOP_VALUE, PieceSquareTables::Bishop, false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteRook),   ROOK_VALUE,   PieceSquareTables::Rook,   false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteQueen),  QUEEN_VALUE,  PieceSquareTables::Queen,  false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteKing),   0,            PieceSquareTables::KingMiddle, false);

    // 2. Accumulate Black Assets
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackPawn),   PAWN_VALUE,   PieceSquareTables::Pawn,   true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackKnight), KNIGHT_VALUE, PieceSquareTables::Knight, true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackBishop), BISHOP_VALUE, PieceSquareTables::Bishop, true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackRook),   ROOK_VALUE,   PieceSquareTables::Rook,   true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackQueen),  QUEEN_VALUE,  PieceSquareTables::Queen,  true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackKing),   0,            PieceSquareTables::KingMiddle, true);

    int perspectiveScore = whiteScore - blackScore;
    int sign = (pos.getSideToMove() == Color::White) ? 1 : -1;
    int finalScore = perspectiveScore * sign;

    // =========================================================================
    // CORRHIST HOOK: Read-only query applying positional error calibration bias
    // =========================================================================
    int correction = s_corrTable.getCorrection(pos.getSideToMove(), pos.getHashKey());
    finalScore += correction;

    if (correction != 0) {
        SearchController::getInstance().getStats().corrApplied++;
    }
    // =========================================================================

    return finalScore;
}

} // namespace Boson