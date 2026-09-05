#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "board/bitboard.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace Boson {

const std::array<std::array<int, 6>, 6> MoveOrderer::MVV_LVA = {{
    {105, 104, 103, 102, 101, 100}, // Victim: Pawn
    {205, 204, 203, 202, 201, 200}, // Victim: Knight
    {305, 304, 303, 302, 301, 300}, // Victim: Bishop
    {405, 404, 403, 402, 401, 400}, // Victim: Rook
    {505, 504, 503, 502, 501, 500}, // Victim: Queen
    {605, 604, 603, 602, 601, 600}  // Victim: King
}};

int MoveOrderer::getPieceIndex(Piece p) noexcept {
    if (p == Piece::None) return 0;
    return static_cast<int>(p) % 6;
}

Piece MoveOrderer::findPieceAtSquare(const Position& pos, Square sq) noexcept {
    if (sq == Square::None) return Piece::None;
    const Bitboard squareMask = Bitboards::getSquareBit(sq);
    for (int p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & squareMask) {
            return static_cast<Piece>(p);
        }
    }
    return Piece::None;
}

void MoveOrderer::scoreAndSortMoves(
    const Position& pos,
    MoveList& moves,
    Move ttMove,
    const std::array<std::array<Move, 2>, 64>& killerMoves,
    const std::array<std::array<uint32_t, 64>, 12>& historyTable,
    int ply,
    Move prevMove
) noexcept {
    std::array<int, 256> scores{};

    Move cmhMove;
    if (prevMove.getRawData() != 0) {
        cmhMove = Search::getCMH().getCounterMove(prevMove.getFromSquare(), prevMove.getToSquare());
        if (cmhMove.getRawData() == 0) {
            Piece prevPiece = findPieceAtSquare(pos, prevMove.getToSquare());
            if (prevPiece != Piece::None) {
                cmhMove = Search::getCMH().getCounterMove(prevPiece, prevMove.getToSquare());
            }
        }
    }
    auto& stats = SearchController::getInstance().getStats();

    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        int score = SCORE_QUIET;

        if (m.getRawData() == ttMove.getRawData() && ttMove.getRawData() != 0) {
            score = SCORE_TT;
        }
        else {
            Piece attacker = findPieceAtSquare(pos, m.getFromSquare());
            Piece victim = findPieceAtSquare(pos, m.getToSquare());

            if (m.isEnPassant() ||
                (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None)) {
                victim = (pos.getSideToMove() == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
            }

            if (victim != Piece::None) {
                int aIdx = getPieceIndex(attacker);
                int vIdx = getPieceIndex(victim);
                
                int seeValue = SEE::evaluate(pos, m.getFromSquare(), m.getToSquare());
                if (seeValue >= 0) {
                    score = SCORE_CAPTURES + MVV_LVA[vIdx][aIdx];
                } else {
                    score = SCORE_LOSING_CAPTURES + seeValue;
                }
            }
            else if (m.isPromotion()) {
                score = SCORE_PROMOTIONS;
            }
            else {
                if (ply < 64) {
                    if (m.getRawData() == killerMoves[ply][0].getRawData()) {
                        score = SCORE_KILLER_1;
                    } else if (m.getRawData() == killerMoves[ply][1].getRawData()) {
                        score = SCORE_KILLER_2;
                    }
                    else {
                        const bool isCmh = (cmhMove.getRawData() != 0 && m.getRawData() == cmhMove.getRawData());
                        if (isCmh) {
                            stats.cmhHits++;
                        }

                        int conthistScore = 0;
                        if (prevMove.getRawData() != 0 && attacker != Piece::None) {
                            conthistScore = Search::getContHist().getScore(attacker, prevMove.getToSquare(), m.getToSquare());
                            if (conthistScore > 0) {
                                stats.conthistHits++;
                            }
                        }

                        if (conthistScore > 0) {
                            score = SCORE_CONTHIST_BASE + std::clamp(conthistScore / 8, 0, 3500);
                        }
                        else if (isCmh) {
                            score = SCORE_COUNTERMOVE;
                        }
                        else if (attacker != Piece::None) {
                            size_t pIdx = static_cast<size_t>(attacker);
                            size_t toIdx = static_cast<size_t>(m.getToSquare());
                            int hist = static_cast<int>(historyTable[pIdx][toIdx]);
                            score = SCORE_QUIET + std::clamp(hist + (conthistScore / 16), 0, 20000);
                        }
                    }
                }
            }
        }
        scores[i] = score;
    }

    // Selection sort
    for (size_t i = 0; i < moves.size(); ++i) {
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (scores[j] > scores[i]) {
                std::swap(scores[i], scores[j]);
                std::swap(moves[i], moves[j]);
            }
        }
    }
}

void MoveOrderer::scoreAndSortTacticalMoves(const Position& pos, MoveList& moves) noexcept {
    std::array<int, 256> scores{};

    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        int score = SCORE_QUIET;

        Piece attacker = findPieceAtSquare(pos, m.getFromSquare());
        Piece victim = findPieceAtSquare(pos, m.getToSquare());

        if (m.isEnPassant() ||
            (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None)) {
            victim = (pos.getSideToMove() == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
        }

        if (victim != Piece::None) {
            int aIdx = getPieceIndex(attacker);
            int vIdx = getPieceIndex(victim);

            int seeValue = SEE::evaluate(pos, m.getFromSquare(), m.getToSquare());
            if (seeValue >= 0) {
                score = SCORE_CAPTURES + MVV_LVA[vIdx][aIdx];
            } else {
                score = SCORE_LOSING_CAPTURES + seeValue;
            }
        }
        else if (m.isPromotion()) {
            score = SCORE_PROMOTIONS + static_cast<int>(m.getPromotionPiece()) * 100;
        }

        scores[i] = score;
    }

    // Stack-only selection sort
    for (size_t i = 0; i < moves.size(); ++i) {
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (scores[j] > scores[i]) {
                std::swap(scores[i], scores[j]);
                std::swap(moves[i], moves[j]);
            }
        }
    }
}

} // namespace Boson