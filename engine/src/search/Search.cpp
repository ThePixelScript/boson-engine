#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "search/LMRPolicy.hpp"
#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include "evaluation/Evaluator.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "board/bitboard.hpp"
#include <iostream>
#include <algorithm>

namespace Boson {

TranspositionTable Search::s_tt(16);
CounterMoveTable Search::s_cmTable; 
ContinuationHistoryTable Search::s_chTable;
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
    return Evaluator::evaluate(pos);
}

int Search::quiescence(Position& pos, int alpha, int beta, int ply) noexcept {
    if (ply >= 63) return evaluate(pos);

    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();
    stats.qNodes++;

    const auto& params = controller.getParams();
    uint64_t checkPeriod = (params.time.nodeCheckPeriod > 0) ? params.time.nodeCheckPeriod : 2048;
    if (stats.qNodes % checkPeriod == 0) {
        controller.checkTime();
    }
    if (controller.shouldStop()) return 0;

    const bool inCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());

    if (!inCheck) {
        int standPat = evaluate(pos);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    }

    MoveList moves;
    if (inCheck) {
        MoveGenerator::generateLegalMoves(pos, moves);
        if (moves.size() == 0) {
            return -MATE + ply;
        }
    } else {
        MoveGenerator::generateTacticalMoves(pos, moves);
        if (moves.size() == 0) {
            return alpha;
        }
    }

    MoveOrderer::scoreAndSortTacticalMoves(pos, moves);

    for (size_t i = 0; i < moves.size(); ++i) {
        if (!inCheck && !moves[i].isPromotion()) {
            if (SEE::evaluate(pos, moves[i].getFromSquare(), moves[i].getToSquare()) < 0) {
                continue;
            }
        }

        UndoState undo;
        MoveExecutor::makeMove(pos, moves[i], undo);
        int moveScore = -quiescence(pos, -beta, -alpha, ply + 1);
        MoveExecutor::undoMove(pos, moves[i], undo);

        if (controller.shouldStop()) return 0;
        if (moveScore >= beta) return beta;
        if (moveScore > alpha) alpha = moveScore;
    }

    return alpha;
}

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply, PVLine& pv, bool allowNull, Move prevMove) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();
    const auto& params = controller.getParams();

    stats.nodes++;
    pv.count = 0;

    uint64_t nodeCheckPeriod = (params.time.nodeCheckPeriod > 0) ? params.time.nodeCheckPeriod : 2048;
    if (stats.nodes % nodeCheckPeriod == 0) {
        controller.checkTime();
        if (controller.getLimits().nodes > 0 && stats.nodes >= static_cast<uint64_t>(controller.getLimits().nodes)) {
            controller.requestStop(StopReason::NodesLimit);
        }
    }
    if (controller.shouldStop()) return 0;

    if (depth <= 0) return quiescence(pos, alpha, beta, ply);

    bool inCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());
    if (inCheck) {
        allowNull = false; 
    }

    int originalAlpha = alpha;
    Move ttMove;
    int ttScore = 0;
    int ttDepth = 0;
    TTNodeType ttType = TTNodeType::Exact;

    bool isPvNode = (beta - alpha > 1);

    int extensions = 0;
    if (isPvNode && inCheck && ply < 64 && depth >= 2) {
        extensions = 1;
    }

    // TT Probe: Query entry unconditionally to populate ttMove for move ordering
    bool ttHit = s_tt.probeEntry(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType);
    if (ttHit) {
        int adjustedScore = scoreFromTT(ttScore, ply);
        if (!isPvNode && ttDepth >= depth) {
            if (ttType == TTNodeType::Exact) {
                stats.ttHits++;
                s_tt.recordCutoff();
                return adjustedScore;
            }
            if (ttType == TTNodeType::LowerBound && adjustedScore >= beta) {
                stats.ttHits++;
                s_tt.recordCutoff();
                return adjustedScore;
            }
            if (ttType == TTNodeType::UpperBound && adjustedScore <= alpha) {
                stats.ttHits++;
                s_tt.recordCutoff();
                return adjustedScore;
            }
        }
    }

    int staticEval = evaluate(pos);

    // Reverse Futures Pruning (RFP)
    if (!isPvNode && !inCheck && depth <= 6 && std::abs(beta) < MATE - 100) {
        int margin = 75 * depth;
        if (staticEval - margin >= beta) return staticEval - margin;
    }

    int R = params.search.nmpReduction; 
    if (params.debug.enableNMP && allowNull && depth >= params.search.nmpMinDepth && !inCheck && staticEval >= beta) {
        if (pos.hasNonPawnMaterial(pos.getSideToMove())) {
            stats.nullAttempts++;
            stats.nullMoveAttempts++;
            
            UndoState nullUndo;
            pos.makeNullMove(nullUndo);
            
            PVLine nullPv;
            int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, nullPv, false, Move());
            
            pos.undoNullMove(nullUndo);

            if (controller.shouldStop()) return 0;

            if (nullScore >= beta) {
                stats.nullCutoffs++;
                stats.nullMoveCutoffs++;
                stats.betaCutoffs++;
                return beta; 
            } else {
                stats.nullFailures++;
                stats.nullMoveFailures++;
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

    MoveOrderer::scoreAndSortMoves(pos, legalMoves, ttMove, s_killerMoves, s_historyTable, ply, prevMove);

    int bestScore = -INF;
    Move bestMove;
    PVLine childPv;
    int movesSearched = 0;

    for (size_t i = 0; i < legalMoves.size(); ++i) {
        const Move& m = legalMoves[i];

        const Bitboard targetBit = Bitboards::getSquareBit(m.getToSquare());
        const bool isCaptureMove = (pos.getTotalOccupancy() & targetBit) != 0 ||
                                   m.isEnPassant() ||
                                   (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);
        const bool isPromotionMove = m.isPromotion();

        Color enemySide = (pos.getSideToMove() == Color::White) ? Color::Black : Color::White;
        Square enemyKingSq = pos.getKingSquare(enemySide);
        bool inEnemyKingZone = (enemyKingSq != Square::None) &&
                               ((MoveGenerator::getKingAttacks(enemyKingSq) & targetBit) != 0);

        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        movesSearched++;

        bool givesCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());

        int searchedDepth = depth - 1 + extensions;
        int score = -INF;
        int reduction = 0;

        // LMR Eligibility Rules:
        // 1. Move is quiet (not a capture, not an en-passant, not a promotion).
        // 2. Side to move is not in check (!inCheck).
        // 3. Move does not give check (!givesCheck).
        // 4. Sufficient search depth (searchedDepth >= 3).
        // 5. Move index is late (movesSearched >= 4, i.e. not an early candidate).
        // 6. Move is not a TT PV move (!isPvMove).
        // 7. Move does not attack the enemy king zone (!inEnemyKingZone).
        const bool isPvMove = (ttMove.getRawData() != 0 && m.getRawData() == ttMove.getRawData());
        const bool isLateMove = (movesSearched >= params.search.lmrMinMoveCount);

        if (params.debug.enableLMR && searchedDepth >= params.search.lmrMinDepth && isLateMove && !isPvMove && !inCheck && !isCaptureMove && !isPromotionMove && !givesCheck && !inEnemyKingZone) {
            reduction = LMRPolicy::getReduction(searchedDepth, movesSearched);

            // Killer Move discount: Proven refutations receive reduced reduction
            const bool isKiller = (ply < 64) && (s_killerMoves[ply][0].getRawData() == m.getRawData() ||
                                                 s_killerMoves[ply][1].getRawData() == m.getRawData());
            if (isKiller) {
                reduction = std::max(0, reduction - 1);
            }
            
            // History Discount: High-history moves indicate positional refutations
            const Bitboard fromBit = Bitboards::getSquareBit(m.getFromSquare());
            for (size_t p = 0; p < 12; ++p) {
                if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
                    size_t pIdx = p;
                    size_t toIdx = static_cast<size_t>(m.getToSquare());
                    if (s_historyTable[pIdx][toIdx] > 4000) {
                        reduction = std::max(0, reduction - 1);
                    }
                    break;
                }
            }

            if (inEnemyKingZone) {
                reduction = 0;
            }

            if (reduction > 0) {
                stats.lmrAttempts++;
                stats.lmrReducedNodes++;
                stats.reducedNodes++;
                score = -negamax(pos, searchedDepth - reduction, -alpha - 1, -alpha, ply + 1, childPv, true, m);
                if (score > alpha) {
                    stats.lmrResearches++;
                    stats.researches++;
                    score = -negamax(pos, searchedDepth, -beta, -alpha, ply + 1, childPv, true, m);
                    if (score > alpha) {
                        stats.successfulResearches++;
                    }
                }
            }
        }

        if (score == -INF) {
            score = -negamax(pos, searchedDepth, -beta, -alpha, ply + 1, childPv, true, m);
        }

        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (controller.shouldStop()) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove = legalMoves[i];
        }
        
        if (score > alpha) {
            alpha = score;
            
            pv.moves[0] = legalMoves[i];
            for (size_t j = 0; j < childPv.count; ++j) {
                if (j + 1 < 64) pv.moves[j + 1] = childPv.moves[j];
            }
            pv.count = childPv.count + 1;
        }

        if (alpha >= beta) {
            stats.betaCutoffs++;
            
            if (!isCaptureMove) {
                if (prevMove.getRawData() != 0) {
                    Move cmhMove = s_cmTable.getCounterMove(prevMove.getFromSquare(), prevMove.getToSquare());
                    if (cmhMove.getRawData() != 0 && m.getRawData() == cmhMove.getRawData()) {
                        stats.cmhCutoffs++;
                    }
                    s_cmTable.store(prevMove.getFromSquare(), prevMove.getToSquare(), m);

                    // Also store by piece if piece is known on prevTo
                    const Bitboard prevToBit = Bitboards::getSquareBit(prevMove.getToSquare());
                    for (size_t p = 0; p < 12; ++p) {
                        if (pos.getPieceBitboard(static_cast<Piece>(p)) & prevToBit) {
                            s_cmTable.store(static_cast<Piece>(p), prevMove.getToSquare(), m);
                            break;
                        }
                    }

                    const Bitboard fromBit = Bitboards::getSquareBit(m.getFromSquare());
                    for (size_t p = 0; p < 12; ++p) {
                        if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
                            stats.conthistCutoffs++;
                            s_chTable.updateScore(static_cast<Piece>(p), prevMove.getToSquare(), m.getToSquare(), depth * depth);
                            break;
                        }
                    }
                }

                if (ply < 64) {
                    if (s_killerMoves[ply][0].getRawData() != m.getRawData()) {
                        s_killerMoves[ply][1] = s_killerMoves[ply][0];
                        s_killerMoves[ply][0] = m;
                    }
                    
                    const Bitboard fromBit = Bitboards::getSquareBit(m.getFromSquare());
                    for (size_t p = 0; p < 12; ++p) {
                        if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
                            size_t pIdx = p;
                            size_t toIdx = static_cast<size_t>(m.getToSquare());

                            // Stockfish Saturating History Gravity
                            constexpr int D = 16384;
                            int bonus = depth * depth;
                            int currentVal = static_cast<int>(s_historyTable[pIdx][toIdx]);
                            
                            int updatedVal = currentVal + bonus - (currentVal * std::abs(bonus) / D);
                            s_historyTable[pIdx][toIdx] = static_cast<uint32_t>(std::clamp(updatedVal, -D, D));
                            break;
                        }
                    }
                }
            }
            break; 
        }
    }

    if (controller.shouldStop()) return 0;

    TTNodeType storeType = TTNodeType::Exact;
    if (bestScore <= originalAlpha)   storeType = TTNodeType::UpperBound;
    else if (bestScore >= beta)      storeType = TTNodeType::LowerBound;

    if (params.debug.enableCorrHist && !inCheck && depth >= 2 && std::abs(bestScore) < MATE - 100) {
        int evalVal = evaluate(pos);
        if (storeType == TTNodeType::Exact || 
            (storeType == TTNodeType::LowerBound && bestScore > evalVal) ||
            (storeType == TTNodeType::UpperBound && bestScore < evalVal)) {
            stats.corrUpdates++;
            Evaluator::getMutableCorrHist().update(pos, depth, bestScore, evalVal);
        }
    }

    Move moveToStore = (storeType == TTNodeType::UpperBound) ? Move() : bestMove;
    s_tt.store(pos.getHashKey(), scoreToTT(bestScore, ply), moveToStore, depth, storeType, 0);

    return bestScore;
}

int Search::searchWithAspiration(Position& pos, int depth, int prevScore, PVLine& pv) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats();
    const auto& params = controller.getParams();

    if (!params.debug.enableAspiration) {
        return negamax(pos, depth, -INF, INF, 0, pv, true, Move());
    }

    int delta = params.search.aspirationInitialDelta;
    int maxDelta = params.search.aspirationMaxDelta;
    int alpha = std::max(-INF, prevScore - delta);
    int beta = std::min(INF, prevScore + delta);
    int score = prevScore;

    while (true) {
        if (controller.getTimeManager().hasTimeLimit()) {
            controller.checkTime();
            if (controller.shouldStop()) return 0;
        }

        score = negamax(pos, depth, alpha, beta, 0, pv, true, Move());
        if (controller.shouldStop()) return 0;

        // If search window was fully opened to [-INF, +INF], complete unconditionally
        if (alpha <= -INF && beta >= INF) {
            stats.aspirationSuccesses++;
            break;
        }

        if (score <= alpha) {
            stats.failLows++;
            stats.aspirationFailLow++;
            stats.researchCount++;
            stats.aspirationResearches++;

            alpha = std::max(-INF, alpha - delta * 2);
            delta *= 2;
            if (delta > maxDelta || alpha <= -INF) {
                alpha = -INF;
                beta = INF;
            }
        }
        else if (score >= beta) {
            stats.failHighs++;
            stats.aspirationFailHigh++;
            stats.researchCount++;
            stats.aspirationResearches++;

            beta = std::min(INF, beta + delta * 2);
            delta *= 2;
            if (delta > maxDelta || beta >= INF) {
                alpha = -INF;
                beta = INF;
            }
        }
        else {
            stats.aspirationSuccesses++;
            break;
        }
    }

    return score;
}

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    SearchLimits limits;
    limits.depth = maxDepth;
    return runSearch(pos, limits);
}

int Search::runSearch(Position& pos, const SearchLimits& limits) noexcept {
    auto& controller = SearchController::getInstance();
    controller.initSearch(limits, pos);
    auto& stats = controller.getStats(); 

    stats.nodes = 0;
    stats.qNodes = 0;
    stats.ttHits = 0;
    stats.failHighs = stats.aspirationFailHigh = 0;
    stats.failLows = stats.aspirationFailLow = 0;
    stats.aspirationSuccesses = 0;
    stats.researchCount = stats.aspirationResearches = 0;
    stats.nullAttempts = stats.nullMoveAttempts = 0;
    stats.nullCutoffs = stats.nullMoveCutoffs = 0;
    stats.nullFailures = stats.nullMoveFailures = 0;
    stats.nullDisabled = 0;
    stats.lmrAttempts = 0;
    stats.lmrReducedNodes = stats.reducedNodes = 0;
    stats.lmrResearches = stats.researches = 0;
    stats.successfulResearches = 0;
    stats.cmhHits = 0;
    stats.cmhCutoffs = 0;
    stats.conthistHits = 0;
    stats.conthistCutoffs = 0;
    stats.normalizationEvents = 0;
    stats.corrApplied = 0;
    stats.corrUpdates = 0;
    stats.corrPositive = 0;
    stats.corrNegative = 0;
    stats.corrTotalMagnitude = 0;
    stats.completedDepth = 0;
    stats.elapsedTimeMs = 0;
    
    stats.pvString.clear(); 
    stats.pvLine.count = 0;

    if (limits.clearTables) {
        s_tt.clear();
        for (auto& row : s_killerMoves) row.fill(Move());
        for (auto& row : s_historyTable) row.fill(0);
        s_cmTable.clear();
        s_chTable.clear();

        Evaluator::getCorrHist().clear(); 
    } 

    std::cout << "[BOSON SEARCH] Running Ordered Alpha-Beta + Aspiration Framework...\n";

    int lastScore = 0;
    PVLine stablePv;
    int maxDepth = (limits.depth > 0) ? limits.depth : 64;

    for (int d = 1; d <= maxDepth; ++d) {
        if (controller.getTimeManager().hasTimeLimit()) {
            if (controller.getElapsedTimeMs() >= controller.getTimeManager().getSoftLimit()) {
                stats.stopReason = StopReason::SoftTimeLimit;
                break;
            }
        }

        // Stockfish Root PV Lock: Store iteration d-1 best move into TT as Exact
        if (d > 1 && stablePv.count > 0) {
            s_tt.store(pos.getHashKey(), scoreToTT(lastScore, 0), stablePv.moves[0], d + 2, TTNodeType::Exact, 0);
        }

        int score = 0;
        PVLine iterationPv;

        if (d >= 2) {
            score = searchWithAspiration(pos, d, lastScore, iterationPv);
        } else {
            score = negamax(pos, d, -INF, INF, 0, iterationPv, true, Move());
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
            currentPvStr += " " + stablePv.moves[i].toString();
        }
        stats.pvString = currentPvStr;
        stats.pvLine = stablePv;

        std::cout << "info depth " << d 
                  << " score cp " << lastScore 
                  << " nodes " << totalNodes 
                  << " nps " << nps 
                  << " time " << stats.elapsedTimeMs 
                  << " pv" << stats.pvString << std::endl;
    }

    if (stats.stopReason == StopReason::None) {
        stats.stopReason = StopReason::MaxDepthReached;
    }

    std::cout << "\n--- Aspiration Optimization Analytics ---\n";
    std::cout << "  -> Total Window Successes : " << stats.aspirationSuccesses << "\n";
    std::cout << "  -> Window Fail Highs       : " << stats.aspirationFailHigh << "\n";
    std::cout << "  -> Window Fail Lows        : " << stats.aspirationFailLow << "\n";
    std::cout << "  -> Total Re-Searches Hit   : " << stats.aspirationResearches << "\n";
    
    std::cout << "\n--- Null Move Pruning Analytics ---\n";
    std::cout << "  -> Null Move Attempts      : " << stats.nullMoveAttempts << "\n";
    std::cout << "  -> Null Move Cutoffs       : " << stats.nullMoveCutoffs << "\n";
    std::cout << "  -> Null Move Failures      : " << stats.nullMoveFailures << "\n";
    std::cout << "  -> Zugzwang Protections    : " << stats.nullDisabled << "\n";
    std::cout << "[BOSON CLOCK] Search Complete. Stop Reason Code: " << static_cast<int>(stats.stopReason) << "\n";
    
    std::cout << "\n--- Late Move Reduction Analytics ---\n";
    std::cout << "  -> LMR Reduction Attempts  : " << stats.lmrAttempts << "\n";
    std::cout << "  -> LMR Reduced Nodes       : " << stats.lmrReducedNodes << "\n";
    
    uint64_t totalLmr = stats.lmrAttempts;
    double researchRate = totalLmr > 0 ? (static_cast<double>(stats.lmrResearches) / totalLmr) * 100.0 : 0.0;
    double successRate = stats.lmrResearches > 0 ? (static_cast<double>(stats.successfulResearches) / stats.lmrResearches) * 100.0 : 0.0;

    std::cout << "  -> Triggered Re-Searches   : " << stats.lmrResearches << " (" << researchRate << "% of reduced nodes)\n";
    std::cout << "  -> Successful PV Overturns : " << stats.successfulResearches << " (" << successRate << "% efficiency)\n";

    std::cout << "\n--- Counter-Move History (CMH) Analytics ---\n";
    std::cout << "  -> CMH Table Hits          : " << stats.cmhHits << "\n";
    std::cout << "  -> CMH Triggered Cutoffs   : " << stats.cmhCutoffs << "\n";
    
    std::cout << "\n--- Continuation History Analytics ---\n";
    std::cout << "  -> Continuation Table Hits : " << stats.conthistHits << "\n";
    std::cout << "  -> Continuation Cutoffs    : " << stats.conthistCutoffs << "\n";
    std::cout << "  -> Table Normalization Evts: " << stats.normalizationEvents << "\n";

    std::cout << "\n--- Correction History Analytics ---\n";
    std::cout << "  -> Bias Corrections Applied: " << stats.corrApplied << " (+:" << stats.corrPositive << " -:" << stats.corrNegative << ")\n";
    std::cout << "  -> Evaluator Bias Updates  : " << stats.corrUpdates << "\n";
    std::cout << "  -> Total Correction Mag    : " << stats.corrTotalMagnitude << " cp\n";
    std::cout << "=================================================================\n";
    
    controller.printBenchmarkReport(maxDepth, 16, 1);

    return lastScore;
}

} // namespace Boson