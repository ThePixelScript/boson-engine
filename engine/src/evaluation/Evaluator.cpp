#include "evaluation/Evaluator.hpp"
#include "evaluation/PieceSquareTables.hpp"
#include "search/SearchController.hpp"
#include <bit>
#include <algorithm>

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

    // 1. Accumulate Non-King White Assets
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhitePawn),   PAWN_VALUE,   PieceSquareTables::Pawn,   false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteKnight), KNIGHT_VALUE, PieceSquareTables::Knight, false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteBishop), BISHOP_VALUE, PieceSquareTables::Bishop, false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteRook),   ROOK_VALUE,   PieceSquareTables::Rook,   false);
    whiteScore += scorePieceType(pos.getPieceBitboard(Piece::WhiteQueen),  QUEEN_VALUE,  PieceSquareTables::Queen,  false);

    // 2. Accumulate Non-King Black Assets
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackPawn),   PAWN_VALUE,   PieceSquareTables::Pawn,   true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackKnight), KNIGHT_VALUE, PieceSquareTables::Knight, true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackBishop), BISHOP_VALUE, PieceSquareTables::Bishop, true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackRook),   ROOK_VALUE,   PieceSquareTables::Rook,   true);
    blackScore += scorePieceType(pos.getPieceBitboard(Piece::BlackQueen),  QUEEN_VALUE,  PieceSquareTables::Queen,  true);

    // 3. Game Phase Tapering
    // Standard phase weights: Knight=1, Bishop=1, Rook=2, Queen=4 (Total = 24)
    int phase = 0;
    phase += std::popcount(pos.getPieceBitboard(Piece::WhiteKnight) | pos.getPieceBitboard(Piece::BlackKnight)) * 1;
    phase += std::popcount(pos.getPieceBitboard(Piece::WhiteBishop) | pos.getPieceBitboard(Piece::BlackBishop)) * 1;
    phase += std::popcount(pos.getPieceBitboard(Piece::WhiteRook)   | pos.getPieceBitboard(Piece::BlackRook))   * 2;
    phase += std::popcount(pos.getPieceBitboard(Piece::WhiteQueen)  | pos.getPieceBitboard(Piece::BlackQueen))  * 4;
    phase = std::clamp(phase, 0, 24);

    // 4. King Positional Evaluation with Phase Tapering
    Bitboard whiteKingBb = pos.getPieceBitboard(Piece::WhiteKing);
    if (whiteKingBb) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, whiteKingBb);
        #else
            sq = __builtin_ctzll(whiteKingBb);
        #endif
        int mg = PieceSquareTables::KingMiddle[sq];
        int eg = PieceSquareTables::KingEndgame[sq];
        whiteScore += (mg * phase + eg * (24 - phase)) / 24;
    }

    Bitboard blackKingBb = pos.getPieceBitboard(Piece::BlackKing);
    if (blackKingBb) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, blackKingBb);
        #else
            sq = __builtin_ctzll(blackKingBb);
        #endif
        size_t pstIndex = static_cast<size_t>(sq) ^ 56;
        int mg = PieceSquareTables::KingMiddle[pstIndex];
        int eg = PieceSquareTables::KingEndgame[pstIndex];
        blackScore += (mg * phase + eg * (24 - phase)) / 24;
    }

    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();
    stats.staticEvalCalls++;

    int perspectiveScore = whiteScore - blackScore;
    int sign = (pos.getSideToMove() == Color::White) ? 1 : -1;
    int finalScore = perspectiveScore * sign;

    const auto& params = controller.getParams();
    if (params.debug.enableCorrHist) {
        // Read-only Correction History bias (zero side effects on external state)
        int corrOffset = s_corrTable.probe(pos);
        finalScore += corrOffset;

        stats.corrApplied++;
        if (corrOffset > 0) stats.corrPositive++;
        else if (corrOffset < 0) stats.corrNegative++;
        stats.corrTotalMagnitude += std::abs(corrOffset);
    }

    return finalScore;
}

} // namespace Boson