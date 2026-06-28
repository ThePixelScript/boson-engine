#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/LMRPolicy.hpp"
#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include "evaluation/Evaluator.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>
#include <algorithm>

namespace Boson {

TranspositionTable Search::s_tt(16);
std::array<std::array<Move, 2>, 64> Search::s_killerMoves{};
std::array<std::array<uint32_t, 64>, 12> Search::s_historyTable{};

uint64_t Search::perft(Position& pos, int depth) noexcept {
    if (depth == 0) return 1ULL;
    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);
    uint64_t nodes = 0;
    for (size_t i = 0; i < legalMoves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        nodes += perft(pos, depth - 1);
        MoveExecutor::undoMove(pos, legalMoves[i], undo);
    }
    return nodes;
}

void Search::divide(Position& pos, int depth) noexcept {
    if (depth == 0) return;
    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);
    std::cout << "\n--- PERFT DIVIDE (Depth " << depth << ") ---\n";
    uint64_t totalNodes = 0;
    for (size_t i = 0; i < legalMoves.size(); ++i) {
        const Move& m = legalMoves[i];
        UndoState undo;
        MoveExecutor::makeMove(pos, m, undo);
        uint64_t nodesForMove = perft(pos, depth - 1);
        totalNodes += nodesForMove;
        MoveExecutor::undoMove(pos, m, undo);

        int from = static_cast<int>(m.getFromSquare());
        int to = static_cast<int>(m.getToSquare());
        std::cout << static_cast<char>('a' + (from % 8)) << static_cast<char>('1' + (from / 8))
                  << static_cast<char>('a' + (to % 8)) << static_cast<char>('1' + (to / 8)) << " : " << nodesForMove << "\n";
    }
    std::cout << "Total Nodes: " << totalNodes << "\n-------------------------\n";
}

int Search::evaluate(const Position& pos) noexcept {
    return Evaluator::evaluate(const_cast<Position&>(pos));
}

int Search::quiescence(Position& pos, int alpha, int beta, int ply) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();

    stats.qNodes++;
    if (stats.qNodes % NODE_CHECK_PERIOD == 0) {
        controller.checkTime();
    }
    if (controller.shouldStop()) return 0;

    int standPat = evaluate(pos);

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    MoveList tacticalMoves;
    MoveGenerator::generateTacticalMoves(pos, tacticalMoves);
    
    static const std::array<std::array<Move, 2>, 64> emptyKillers{};
    static const std::array<std::array<uint32_t, 64>, 12> emptyHistory{};
    MoveOrderer::scoreAndSortMoves(pos, tacticalMoves, Move(), emptyKillers, emptyHistory, ply);

    for (size_t i = 0; i < tacticalMoves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(pos, tacticalMoves[i], undo);
        int score = -quiescence(pos, -beta, -alpha, ply + 1);
        MoveExecutor::undoMove(pos, tacticalMoves[i], undo);

        if (controller.shouldStop()) return 0;

        if (score >= beta) return beta;
        if (score > alpha) alpha = standPat;
    }

    return alpha;
}

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply, PVLine& pv, bool allowNull) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();

    stats.nodes++;
    pv.count = 0;

    if (stats.nodes % NODE_CHECK_PERIOD == 0) {
        controller.checkTime();
    }
    if (controller.shouldStop()) return 0;

    if (depth == 0) return quiescence(pos, alpha, beta, ply);

    bool inCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());
    if (inCheck) {
        allowNull = false; 
    }

    int originalAlpha = alpha;
    Move ttMove;
    int ttScore = 0;
    int ttDepth = 0;
    TTNodeType ttType;

    if (s_tt.probe(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType, alpha, beta)) {
        if (ttDepth >= depth) {
            stats.ttHits++;
            return ttScore;
        }
    }

    // =========================================================================
    // MODULE 6.6: NULL MOVE PRUNING (NMP)
    // =========================================================================
    constexpr int R = 2; 
    if (allowNull && depth >= 3 && !inCheck) {
        Color side = pos.getSideToMove();
        Bitboard nonPawnMaterial = (side == Color::White) 
            ? (pos.getPieceBitboard(Piece::WhiteKnight) | pos.getPieceBitboard(Piece::WhiteBishop) |
               pos.getPieceBitboard(Piece::WhiteRook)   | pos.getPieceBitboard(Piece::WhiteQueen))
            : (pos.getPieceBitboard(Piece::BlackKnight) | pos.getPieceBitboard(Piece::BlackBishop) |
               pos.getPieceBitboard(Piece::BlackRook)   | pos.getPieceBitboard(Piece::BlackQueen));

        if (nonPawnMaterial != 0) {
            stats.nullAttempts++;
            
            // Inline Null Move Simulation Block
            Color originalSide = pos.getSideToMove();
            Square originalEp = pos.getEnPassantSquare();
            
            pos.setSideToMove((originalSide == Color::White) ? Color::Black : Color::White);
            pos.setEnPassantSquare(Square::None);
            
            PVLine nullPv;
            int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, nullPv, false);
            
            // Restore original position state invariants
            pos.setSideToMove(originalSide);
            pos.setEnPassantSquare(originalEp);

            if (controller.shouldStop()) return 0;

            if (nullScore >= beta) {
                stats.nullCutoffs++;
                stats.betaCutoffs++;
                s_tt.store(pos.getHashKey(), beta, Move(), depth, TTNodeType::LowerBound, 0);
                return beta; 
            } else {
                stats.nullFailures++;
            }
        } else {
            stats.nullDisabled++;
        }
    }

    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);

    if (legalMoves.size() == 0) {
        if (inCheck) return -MATE + ply; 
        return 0; 
    }

    MoveOrderer::scoreAndSortMoves(pos, legalMoves, ttMove, s_killerMoves, s_historyTable, ply);

    int bestScore = -INF;
    Move bestMove;
    PVLine childPv;
    int movesSearched = 0;

    for (size_t i = 0; i < legalMoves.size(); ++i) {
        const Move& m = legalMoves[i];
        
        // Distinguish tactical captures/promotions from quiet moves using target occupancies
        Bitboard targetBit = 1ULL << static_cast<size_t>(m.getToSquare());
        bool isCaptureMove = (pos.getTotalOccupancy() & targetBit) || 
                             (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);
        
bool isPromotionMove = ((m.getFromSquare() >= Square::A7 && m.getFromSquare() <= Square::H7 && pos.getPieceBitboard(Piece::WhitePawn) & (1ULL << static_cast<size_t>(m.getFromSquare()))) ||
                                (m.getFromSquare() >= Square::A2 && m.getFromSquare() <= Square::H2 && pos.getPieceBitboard(Piece::BlackPawn) & (1ULL << static_cast<size_t>(m.getFromSquare())))) &&
                               (static_cast<int>(m.getToSquare()) / 8 == 7 || static_cast<int>(m.getToSquare()) / 8 == 0); // <-- CAST TO INT BEFORE DIVISION
        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        movesSearched++;

        int score = -INF;
        int reduction = 0;

        // =========================================================================
        // MODULE 6.7: LATE MOVE REDUCTION (LMR) CONTROLLER STEP
        // =========================================================================
        if (depth >= 3 && movesSearched >= 4 && !inCheck && !isCaptureMove && !isPromotionMove) {
            reduction = LMRPolicy::getReduction(depth, movesSearched);
            
            if (reduction > 0) {
                stats.lmrAttempts++;
                // Step A: Search at reduced depth using a narrow scout null-window
                score = -negamax(pos, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, childPv, true);
                
                if (score > alpha) {
                    stats.researches++;
                    // Node anomaly detected! Cancel reduction but keep scout window
                    score = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1, childPv, true);
                }
            }
        }

        // Step B: Full search fallback if LMR was skipped or if a scout search failed high
        if (score == -INF || (score > alpha && reduction > 0)) {
            score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, childPv, true);
        }
        // =========================================================================

        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (controller.shouldStop()) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove = legalMoves[i];
        }
        
        if (score > alpha) {
            if (reduction > 0) stats.successfulResearches++;
            alpha = score;
            
            pv.moves[0] = legalMoves[i];
            for (size_t j = 0; j < childPv.count; ++j) {
                if (j + 1 < 64) pv.moves[j + 1] = childPv.moves[j];
            }
            pv.count = childPv.count + 1;
        }

        if (alpha >= beta) {
            stats.betaCutoffs++;
            if (!isCaptureMove && ply < 64) {
                if (s_killerMoves[ply][0].getRawData() != legalMoves[i].getRawData()) {
                    s_killerMoves[ply][1] = s_killerMoves[ply][0];
                    s_killerMoves[ply][0] = legalMoves[i];
                }
                for (size_t p = 0; p < 12; ++p) {
                    if (pos.getPieceBitboard(static_cast<Piece>(p)) & (1ULL << static_cast<size_t>(legalMoves[i].getFromSquare()))) {
                        s_historyTable[p][static_cast<size_t>(legalMoves[i].getToSquare())] += depth * depth;
                        break;
                    }
                }
            }
            break; 
        }
    }

    if (controller.shouldStop()) return 0;

    TTNodeType storeType = TTNodeType::Exact;
    if (bestScore <= originalAlpha)  storeType = TTNodeType::UpperBound;
    else if (bestScore >= beta)      storeType = TTNodeType::LowerBound;

    s_tt.store(pos.getHashKey(), bestScore, bestMove, depth, storeType, 0);
    return bestScore;
}

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();

    s_tt.clear();
    for (auto& row : s_killerMoves) row.fill(Move());
    for (auto& row : s_historyTable) row.fill(0);

    std::cout << "[BOSON SEARCH] Running Ordered Alpha-Beta + Aspiration Framework...\n";

    int lastScore = 0;
    PVLine stablePv;
    int delta = 30;

    for (int d = 1; d <= maxDepth; ++d) {
        if (controller.getTimeManager().hasTimeLimit()) {
            if (controller.getElapsedTimeMs() >= controller.getTimeManager().getSoftLimit()) {
                stats.stopReason = StopReason::SoftTimeLimit;
                break;
            }
        }

        int score = 0;
        PVLine iterationPv;
        
        if (d >= 5) {
            int alphaWindow = lastScore - delta;
            int betaWindow = lastScore + delta;
            int researchAttemptsAtThisDepth = 0;

            while (true) {
                if (controller.getTimeManager().hasTimeLimit()) {
                    controller.checkTime();
                    if (controller.shouldStop()) break;
                }

                score = negamax(pos, d, alphaWindow, betaWindow, 0, iterationPv, true);
                if (controller.shouldStop()) break;

                if (score <= alphaWindow) {
                    stats.failLows++;
                    stats.researchCount++;
                    researchAttemptsAtThisDepth++;
                    betaWindow = (alphaWindow + betaWindow) / 2;
                    alphaWindow = lastScore - (delta * (1 << researchAttemptsAtThisDepth));
                    if (alphaWindow <= -INF) alphaWindow = -INF;
                }
                else if (score >= betaWindow) {
                    stats.failHighs++;
                    stats.researchCount++;
                    researchAttemptsAtThisDepth++;
                    alphaWindow = (alphaWindow + betaWindow) / 2;
                    betaWindow = lastScore + (delta * (1 << researchAttemptsAtThisDepth));
                    if (betaWindow >= INF) betaWindow = INF;
                }
                else {
                    stats.aspirationSuccesses++;
                    break;
                }

                if (researchAttemptsAtThisDepth >= 4) {
                    alphaWindow = -INF;
                    betaWindow = INF;
                }
            }
        } else {
            score = negamax(pos, d, -INF, INF, 0, iterationPv, true);
        }

        if (controller.shouldStop()) {
            break;
        }

        lastScore = score;
        stablePv = iterationPv;
        stats.completedDepth = d;
        stats.elapsedTimeMs = controller.getElapsedTimeMs();

        uint64_t totalNodes = stats.nodes + stats.qNodes;
        uint64_t nps = stats.elapsedTimeMs > 0 ? (totalNodes * 1000) / stats.elapsedTimeMs : totalNodes;

        std::string currentPvStr = "";
        for (size_t i = 0; i < stablePv.count; ++i) {
            int from = static_cast<int>(stablePv.moves[i].getFromSquare());
            int to = static_cast<int>(stablePv.moves[i].getToSquare());
            currentPvStr += " " + std::string(1, static_cast<char>('a' + (from % 8))) + 
                                  std::string(1, static_cast<char>('1' + (from / 8))) +
                                  std::string(1, static_cast<char>('a' + (to % 8))) + 
                                  std::string(1, static_cast<char>('1' + (to / 8)));
        }
        stats.pvString = currentPvStr;

        std::cout << "info depth " << d 
                  << " score cp " << lastScore 
                  << " nodes " << totalNodes 
                  << " nps " << nps 
                  << " time " << stats.elapsedTimeMs 
                  << " pv" << stats.pvString << "\n";
    }

    if (stats.stopReason == StopReason::None) {
        stats.stopReason = StopReason::MaxDepthReached;
    }

    std::cout << "\n--- Aspiration Optimization Analytics ---\n";
    std::cout << "  -> Total Window Successes : " << stats.aspirationSuccesses << "\n";
    std::cout << "  -> Window Fail Highs       : " << stats.failHighs << "\n";
    std::cout << "  -> Window Fail Lows        : " << stats.failLows << "\n";
    std::cout << "  -> Total Re-Searches Hit   : " << stats.researchCount << "\n";
    
    std::cout << "\n--- Null Move Pruning Analytics ---\n";
    std::cout << "  -> Null Move Attempts      : " << stats.nullAttempts << "\n";
    std::cout << "  -> Null Move Cutoffs       : " << stats.nullCutoffs << "\n";
    std::cout << "  -> Null Move Failures      : " << stats.nullFailures << "\n";
    std::cout << "  -> Zugzwang Protections    : " << stats.nullDisabled << "\n";
    std::cout << "[BOSON CLOCK] Search Complete. Stop Reason Code: " << static_cast<int>(stats.stopReason) << "\n";
    
    std::cout << "\n--- Late Move Reduction Analytics ---\n";
    std::cout << "  -> LMR Reduction Attempts  : " << stats.lmrAttempts << "\n";
    
    // Explicit safety check to prevent divide-by-zero outputs on quick shallow runs
    uint64_t totalLmr = stats.lmrAttempts;
    double researchRate = totalLmr > 0 ? (static_cast<double>(stats.researches) / totalLmr) * 100.0 : 0.0;
    double successRate = stats.researches > 0 ? (static_cast<double>(stats.successfulResearches) / stats.researches) * 100.0 : 0.0;

    std::cout << "  -> Triggered Re-Searches   : " << stats.researches << " (" << researchRate << "% of reduced nodes)\n";
    std::cout << "  -> Successful PV Overturns : " << stats.successfulResearches << " (" << successRate << "% efficiency)\n";

    return lastScore;
}

} // namespace Boson