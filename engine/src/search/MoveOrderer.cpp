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

    Move cmhMove = Search::getCMH().getCounterMove(prevMove.getFromSquare(), prevMove.getToSquare());
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
                
                int mvvLvaScore = SCORE_CAPTURES + MVV_LVA[vIdx][aIdx];
                int seeValue = SEE::evaluate(const_cast<Position&>(pos), m.getFromSquare(), m.getToSquare());

                if (aIdx == 0 && vIdx == 0 && seeValue == 0) {
                    score = SCORE_QUIET + 5000;
                } else if (seeValue < -400) {
                    score = SCORE_QUIET - 5000 + seeValue;
                } else {
                    score = mvvLvaScore;
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
                    else if (cmhMove.getRawData() != 0 && m.getRawData() == cmhMove.getRawData()) {
                        stats.cmhHits++;
                        score = SCORE_KILLER_2 - 50;
                    }
                    else if (attacker != Piece::None) {
                        size_t pIdx = static_cast<size_t>(attacker);
                        size_t toIdx = static_cast<size_t>(m.getToSquare());
                        
                        int conthistScore = 0;
                        if (prevMove.getRawData() != 0) {
                            conthistScore = Search::getContHist().getScore(attacker, prevMove.getToSquare(), m.getToSquare());
                            if (conthistScore > 0) {
                                stats.conthistHits++;
                            }
                        }

                        score = SCORE_QUIET + static_cast<int>(historyTable[pIdx][toIdx]) + (conthistScore / 16);

                        if (attacker == Piece::WhitePawn || attacker == Piece::BlackPawn) {
                            Square toSq = m.getToSquare();
                            if (toSq == Square::E3 || toSq == Square::E4 || 
                                toSq == Square::D3 || toSq == Square::D4 || 
                                toSq == Square::A6 || toSq == Square::A3) {
                                score += 6000; // Sorts quiet pawn setups above passive major piece shuffles
                            }
                        }

                        if (attacker == Piece::WhiteKing || attacker == Piece::BlackKing) {
                            if (!m.isCastling()) {
                                score -= 3000;
                            }
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

} // namespace Boson