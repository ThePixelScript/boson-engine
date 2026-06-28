#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace Boson {

// 1. Instantiate the static MVV_LVA matrix member declared in the class header
const std::array<std::array<int, 6>, 6> MoveOrderer::MVV_LVA = {{
    {105, 104, 103, 102, 101, 100}, // Victim: Pawn
    {205, 204, 203, 202, 201, 200}, // Victim: Knight
    {305, 304, 303, 302, 301, 300}, // Victim: Bishop
    {405, 404, 403, 402, 401, 400}, // Victim: Rook
    {505, 504, 503, 502, 501, 500}, // Victim: Queen
    {605, 604, 603, 602, 601, 600}  // Victim: King
}};

// 2. Define the static member helper functions properly qualified under MoveOrderer::
int MoveOrderer::getPieceIndex(Piece p) noexcept {
    if (p == Piece::None) return 0;
    return static_cast<int>(p) % 6;
}

Piece MoveOrderer::findPieceAtSquare(const Position& pos, Square sq) noexcept {
    for (int p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & (1ULL << static_cast<size_t>(sq))) {
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
    
    // Read Contextual Counter Move from Search
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
            
            if (victim != Piece::None) {
                int aIdx = getPieceIndex(attacker);
                int vIdx = getPieceIndex(victim);
                int mvvLvaScore = SCORE_CAPTURES + MVV_LVA[aIdx][vIdx];
                int seeValue = SEE::evaluate(const_cast<Position&>(pos), m.getFromSquare(), m.getToSquare());

                if (seeValue < 0) score = SCORE_QUIET - 10000 + seeValue;
                else             score = mvvLvaScore;
            }
            else if (((attacker == Piece::WhitePawn && m.getToSquare() >= Square::A8 && m.getToSquare() <= Square::H8) ||
                      (attacker == Piece::BlackPawn && m.getToSquare() >= Square::A1 && m.getToSquare() <= Square::H1)) &&
                     (static_cast<int>(m.getToSquare()) / 8 == 7 || static_cast<int>(m.getToSquare()) / 8 == 0)) {
                score = SCORE_PROMOTIONS;
            }
            else {
                if (ply < 64) {
                    if (m.getRawData() == killerMoves[ply][0].getRawData()) {
                        score = SCORE_KILLER_1;
                    } else if (m.getRawData() == killerMoves[ply][1].getRawData()) {
                        score = SCORE_KILLER_2;
                    } 
                    // Counter Move Integration Priority Slot
                    else if (cmhMove.getRawData() != 0 && m.getRawData() == cmhMove.getRawData()) {
                        stats.cmhHits++;
                        score = SCORE_KILLER_2 - 100; 
                    } 
                    else if (attacker != Piece::None) {
                        size_t pIdx = static_cast<size_t>(attacker);
                        size_t toIdx = static_cast<size_t>(m.getToSquare());
                        score = static_cast<int>(historyTable[pIdx][toIdx]);
                    }
                }
            }
        }
        scores[i] = score;
    }

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