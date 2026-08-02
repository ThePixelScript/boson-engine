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

    const bool inCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());

    if (!inCheck) {
        int standPat = evaluate(pos);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    }

    MoveList moves;
    if (inCheck) {
        MoveGenerator::generateLegalMoves(pos, moves);
    } else {
        MoveGenerator::generateTacticalMoves(pos, moves);
    }

    auto getPieceValue = [](Piece p) -> int {
        switch (p) {
            case Piece::WhitePawn:   case Piece::BlackPawn:   return 100;
            case Piece::WhiteKnight: case Piece::BlackKnight: return 300;
            case Piece::WhiteBishop: case Piece::BlackBishop: return 325;
            case Piece::WhiteRook:   case Piece::BlackRook:   return 500;
            case Piece::WhiteQueen:  case Piece::BlackQueen:  return 900;
            case Piece::WhiteKing:   case Piece::BlackKing:   return 20000;
            default: return 0;
        }
    };

    for (size_t i = 0; i < moves.size(); ++i) {
        size_t bestIdx = i;
        int maxVictim = -1;

        for (size_t j = i; j < moves.size(); ++j) {
            int victimValue = 0;
            Bitboard targetBit = Bitboards::getSquareBit(moves[j].getToSquare());

            for (size_t p = 0; p < 12; ++p) {
                if (pos.getPieceBitboard(static_cast<Piece>(p)) & targetBit) {
                    victimValue = getPieceValue(static_cast<Piece>(p));
                    break;
                }
            }

            if (victimValue > maxVictim) {
                maxVictim = victimValue;
                bestIdx = j;
            }
        }

        if (bestIdx != i) {
            std::swap(moves[i], moves[bestIdx]);
        }
    }

    for (size_t i = 0; i < moves.size(); ++i) {
        if (!inCheck && !moves[i].isPromotion()) {
            if (SEE::evaluate(pos, moves[i].getFromSquare(), moves[i].getToSquare()) < -200) {
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

    bool isPvNode = (beta - alpha > 1);

    if (!isPvNode && s_tt.probe(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType, alpha, beta)) {
        if (ttDepth >= depth) {
            stats.ttHits++;
            return ttScore;
        }
    }

    int staticEval = evaluate(pos);
    constexpr int R = 2; 
    if (allowNull && depth >= 3 && !inCheck && staticEval >= beta) {
        Color side = pos.getSideToMove();
        Bitboard nonPawnMaterial = (side == Color::White) 
            ? (pos.getPieceBitboard(Piece::WhiteKnight) | pos.getPieceBitboard(Piece::WhiteBishop) |
               pos.getPieceBitboard(Piece::WhiteRook)   | pos.getPieceBitboard(Piece::WhiteQueen))
            : (pos.getPieceBitboard(Piece::BlackKnight) | pos.getPieceBitboard(Piece::BlackBishop) |
               pos.getPieceBitboard(Piece::BlackRook)   | pos.getPieceBitboard(Piece::BlackQueen));

        if (nonPawnMaterial != 0) {
            stats.nullAttempts++;
            
            Color originalSide = pos.getSideToMove();
            Square originalEp = pos.getEnPassantSquare();
            
            pos.setSideToMove((originalSide == Color::White) ? Color::Black : Color::White);
            pos.setEnPassantSquare(Square::None);
            
            PVLine nullPv;
            int nullScore = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, nullPv, false, Move());
            
            pos.setSideToMove(originalSide);
            pos.setEnPassantSquare(originalEp);

            if (controller.shouldStop()) return 0;

            if (nullScore >= beta) {
                stats.nullCutoffs++;
                stats.betaCutoffs++;
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

        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        movesSearched++;

        bool givesCheck = MoveGenerator::inCheck(pos, pos.getSideToMove());

        int searchedDepth = depth - 1;
        int score = -INF;
        int reduction = 0;

        // LMR Guard: Do NOT reduce root PV moves (ply == 0) or early candidates
        bool isPvMove = (ttMove.getRawData() != 0 && m.getRawData() == ttMove.getRawData());
        bool isEarlyMove = (movesSearched <= 3);

        if (ply > 0 && searchedDepth >= 3 && !isEarlyMove && !isPvMove && !inCheck && !isCaptureMove && !isPromotionMove && !givesCheck) {
            reduction = LMRPolicy::getReduction(searchedDepth, movesSearched);
            
            // History Discount
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

            if (reduction > 0) {
                stats.lmrAttempts++;
                score = -negamax(pos, searchedDepth - reduction, -alpha - 1, -alpha, ply + 1, childPv, true, m);
                if (score > alpha) {
                    stats.researches++;
                    score = -negamax(pos, searchedDepth, -beta, -alpha, ply + 1, childPv, true, m);
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
            
            if (prevMove.getRawData() != 0) {
                Move cmhMove = s_cmTable.getCounterMove(prevMove.getFromSquare(), prevMove.getToSquare());
                if (cmhMove.getRawData() != 0 && m.getRawData() == cmhMove.getRawData()) {
                    stats.cmhCutoffs++;
                }
                s_cmTable.store(prevMove.getFromSquare(), prevMove.getToSquare(), m);

                const Bitboard fromBit = Bitboards::getSquareBit(m.getFromSquare());
                for (size_t p = 0; p < 12; ++p) {
                    if (pos.getPieceBitboard(static_cast<Piece>(p)) & fromBit) {
                        stats.conthistCutoffs++;
                        s_chTable.updateScore(static_cast<Piece>(p), prevMove.getToSquare(), m.getToSquare(), depth * depth);
                        break;
                    }
                }
            }

            if (!isCaptureMove && ply < 64) {
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
            break; 
        }
    }

    if (controller.shouldStop()) return 0;

    TTNodeType storeType = TTNodeType::Exact;
    if (bestScore <= originalAlpha)   storeType = TTNodeType::UpperBound;
    else if (bestScore >= beta)      storeType = TTNodeType::LowerBound;

    if (storeType == TTNodeType::Exact && !inCheck && depth >= 4) {
        int evalVal = evaluate(pos);
        stats.corrUpdates++;
        Evaluator::getCorrHist().updateCorrection(pos.getSideToMove(), pos.getHashKey(), depth, bestScore, evalVal);
    }

    Move moveToStore = (storeType == TTNodeType::UpperBound) ? Move() : bestMove;
    s_tt.store(pos.getHashKey(), bestScore, moveToStore, depth, storeType, 0);

    return bestScore;
}

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    auto& controller = SearchController::getInstance();
    auto& stats = controller.getStats(); 

    stats.nodes = 0;
    stats.qNodes = 0;
    stats.ttHits = 0;
    stats.failHighs = 0;
    stats.failLows = 0;
    stats.aspirationSuccesses = 0;
    stats.researchCount = 0;
    stats.nullAttempts = 0;
    stats.nullCutoffs = 0;
    stats.nullFailures = 0;
    stats.nullDisabled = 0;
    stats.lmrAttempts = 0;
    stats.researches = 0;
    stats.successfulResearches = 0;
    stats.cmhHits = 0;
    stats.cmhCutoffs = 0;
    stats.conthistHits = 0;
    stats.conthistCutoffs = 0;
    stats.normalizationEvents = 0;
    stats.corrApplied = 0;
    stats.corrUpdates = 0;
    stats.completedDepth = 0;
    stats.elapsedTimeMs = 0;
    
    stats.pvString.clear(); 

    s_tt.clear();
    for (auto& row : s_killerMoves) row.fill(Move());
    for (auto& row : s_historyTable) row.fill(0);
    s_cmTable.clear();
    s_chTable.clear();

    Evaluator::getCorrHist().clear(); 

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

        // Stockfish Root PV Lock: Store iteration d-1 best move into TT as Exact
        if (d > 1 && stablePv.count > 0) {
            s_tt.store(pos.getHashKey(), lastScore, stablePv.moves[0], d + 2, TTNodeType::Exact, 0);
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

                score = negamax(pos, d, alphaWindow, betaWindow, 0, iterationPv, true, Move());
                if (controller.shouldStop()) break;

                if (score <= alphaWindow) {
                    stats.failLows++;
                    stats.researchCount++;
                    researchAttemptsAtThisDepth++;
                    alphaWindow = lastScore - (delta * (1 << researchAttemptsAtThisDepth));
                    if (alphaWindow <= -INF) alphaWindow = -INF;
                }
                else if (score >= betaWindow) {
                    stats.failHighs++;
                    stats.researchCount++;
                    researchAttemptsAtThisDepth++;
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
    
    uint64_t totalLmr = stats.lmrAttempts;
    double researchRate = totalLmr > 0 ? (static_cast<double>(stats.researches) / totalLmr) * 100.0 : 0.0;
    double successRate = stats.researches > 0 ? (static_cast<double>(stats.successfulResearches) / stats.researches) * 100.0 : 0.0;

    std::cout << "  -> Triggered Re-Searches   : " << stats.researches << " (" << researchRate << "% of reduced nodes)\n";
    std::cout << "  -> Successful PV Overturns : " << stats.successfulResearches << " (" << successRate << "% efficiency)\n";

    std::cout << "\n--- Counter-Move History (CMH) Analytics ---\n";
    std::cout << "  -> CMH Table Hits          : " << stats.cmhHits << "\n";
    std::cout << "  -> CMH Triggered Cutoffs   : " << stats.cmhCutoffs << "\n";
    
    std::cout << "\n--- Continuation History Analytics ---\n";
    std::cout << "  -> Continuation Table Hits : " << stats.conthistHits << "\n";
    std::cout << "  -> Continuation Cutoffs    : " << stats.conthistCutoffs << "\n";
    std::cout << "  -> Table Normalization Evts: " << stats.normalizationEvents << "\n";

    std::cout << "\n--- Correction History Analytics ---\n";
    std::cout << "  -> Bias Corrections Applied: " << stats.corrApplied << "\n";
    std::cout << "  -> Evaluator Bias Updates  : " << stats.corrUpdates << "\n";
    std::cout << "=================================================================\n";
    
    return lastScore;
}

} // namespace Boson