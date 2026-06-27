#include "search/Search.hpp"
#include "search/MoveOrderer.hpp"
#include "evaluation/Evaluator.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>
#include <chrono>

namespace Boson {

TranspositionTable Search::s_tt(16);
uint64_t Search::m_nodes = 0;
uint64_t Search::m_qNodes = 0;

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

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply, PVLine& pv) noexcept {
    m_nodes++;
    pv.count = 0;

    if (depth == 0) {
        return quiescence(pos, alpha, beta, ply);
    }

    int originalAlpha = alpha;
    Move ttMove;
    int ttScore = 0;
    int ttDepth = 0;
    TTNodeType ttType;

    if (s_tt.probe(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType, alpha, beta)) {
        if (ttDepth >= depth) return ttScore;
    }

    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);

    if (legalMoves.size() == 0) {
        if (MoveGenerator::inCheck(pos, pos.getSideToMove())) return -MATE + ply; 
        return 0; 
    }

    MoveOrderer::scoreAndSortMoves(pos, legalMoves, ttMove, s_killerMoves, s_historyTable, ply);

    int bestScore = -INF;
    Move bestMove;
    PVLine childPv;

    for (size_t i = 0; i < legalMoves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        
        int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, childPv);
        
        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (score > bestScore) {
            bestScore = score;
            bestMove = legalMoves[i];
        }
        
        if (score > alpha) {
            alpha = score;
            
            // Build the Principal Variation Line by prepending the current best move
            pv.moves[0] = legalMoves[i];
            for (size_t j = 0; j < childPv.count; ++j) {
                if (j + 1 < 64) pv.moves[j + 1] = childPv.moves[j];
            }
            pv.count = childPv.count + 1;
        }

        if (alpha >= beta) {
            Bitboard targetBit = 1ULL << static_cast<size_t>(legalMoves[i].getToSquare());
            bool isCaptureMove = (pos.getTotalOccupancy() & targetBit) || (legalMoves[i].getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);
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

    TTNodeType storeType = TTNodeType::Exact;
    if (bestScore <= originalAlpha)  storeType = TTNodeType::UpperBound;
    else if (bestScore >= beta)      storeType = TTNodeType::LowerBound;

    s_tt.store(pos.getHashKey(), bestScore, bestMove, depth, storeType, 0);
    return bestScore;
}

int Search::quiescence(Position& pos, int alpha, int beta, int ply) noexcept {
    m_qNodes++;
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

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    m_nodes = 0;
    m_qNodes = 0;
    s_tt.clear();
    
    for (auto& row : s_killerMoves) row.fill(Move());
    for (auto& row : s_historyTable) row.fill(0);

    std::cout << "[BOSON SEARCH] Starting Milestone 6 Tournament Engine Architecture...\n";
    auto startTime = std::chrono::high_resolution_clock::now();

    int score = 0;
    PVLine mainPv;

    // Iterative Deepening Loop Framework
    for (int d = 1; d <= maxDepth; ++d) {
        PVLine iterationPv;
        score = negamax(pos, d, -INF, INF, 0, iterationPv);
        
        if (iterationPv.count > 0) {
            mainPv = iterationPv;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        uint64_t totalNodes = m_nodes + m_qNodes;
        uint64_t nps = duration > 0 ? (totalNodes * 1000) / duration : totalNodes;

        // Formal Tournament-Engine Dashboard Output format (Aligns with UCI expectations)
        std::cout << "info depth " << d 
                  << " score cp " << score 
                  << " nodes " << totalNodes 
                  << " nps " << nps 
                  << " time " << duration 
                  << " pv";
        
        for (size_t i = 0; i < mainPv.count; ++i) {
            int from = static_cast<int>(mainPv.moves[i].getFromSquare());
            int to = static_cast<int>(mainPv.moves[i].getToSquare());
            std::cout << " " << static_cast<char>('a' + (from % 8)) << static_cast<char>('1' + (from / 8))
                      << static_cast<char>('a' + (to % 8)) << static_cast<char>('1' + (to / 8));
        }
        std::cout << "\n";
    }
    
    return score;
}

} // namespace Boson