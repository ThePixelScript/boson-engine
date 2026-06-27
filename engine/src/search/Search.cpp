#include "search/Search.hpp"
#include "search/MoveOrderer.hpp"
#include "evaluation/Evaluator.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>

namespace Boson {

TranspositionTable Search::s_tt(16);
uint64_t Search::m_nodes = 0;
uint64_t Search::m_qNodes = 0;

std::array<std::array<Move, 2>, 64> Search::s_killerMoves{};
std::array<std::array<uint32_t, 64>, 12> Search::s_historyTable{};

// ... keep perft(), divide(), evaluate() identical ...

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept {
    m_nodes++;

    // Base Case Contract: Delegate explicitly to tactical quiescence validation loops
    if (depth == 0) return quiescence(pos, alpha, beta, ply);

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

    for (size_t i = 0; i < legalMoves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (score > bestScore) {
            bestScore = score;
            bestMove = legalMoves[i];
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            // Killer / History mutations
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

// Phase AB — Quiescence Search Implementation
int Search::quiescence(Position& pos, int alpha, int beta, int ply) noexcept {
    m_qNodes++;

    // 1. Establish Stand-Pat Baseline Score
    int standPat = evaluate(pos);

    // Alpha-Beta bounding conditions
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    // 2. Generate and order tactical captures only
    MoveList tacticalMoves;
    MoveGenerator::generateTacticalMoves(pos, tacticalMoves);
    
    // Pass empty killers and history tables since q-search evaluates captures exclusively
    static const std::array<std::array<Move, 2>, 64> emptyKillers{};
    static const std::array<std::array<uint32_t, 64>, 12> emptyHistory{};
    MoveOrderer::scoreAndSortMoves(pos, tacticalMoves, Move(), emptyKillers, emptyHistory, ply);

    // 3. Evaluate tactical continuations recursively
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
    int score = 0;
    m_nodes = 0;
    m_qNodes = 0; // Clear instrumentation counts
    s_tt.clear();
    
    for (auto& row : s_killerMoves) row.fill(Move());
    for (auto& row : s_historyTable) row.fill(0);

    std::cout << "[BOSON SEARCH] Running Ordered Alpha-Beta + Quiescence Framework...\n";
    
    for (int d = 1; d <= maxDepth; ++d) {
        score = negamax(pos, d, -INF, INF, 0);
        std::cout << "  -> Depth " << d << " Complete. Score: " << score 
                  << " | Base Nodes: " << m_nodes << " | Q-Nodes: " << m_qNodes << "\n";
    }
    
    return score;
}

int Search::evaluate(const Position& pos) noexcept {
    // Cast away const safely to bridge to the stateless evaluator pipeline
    return Evaluator::evaluate(const_cast<Position&>(pos));
}

} // namespace Boson