#include "search/Search.hpp"
#include "search/MoveOrderer.hpp"
#include "evaluation/Evaluator.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>

namespace Boson {

TranspositionTable Search::s_tt(16);
uint64_t Search::m_nodes = 0;

// Initialize structural sorting data tables
std::array<std::array<Move, 2>, 64> Search::s_killerMoves{};
std::array<std::array<uint32_t, 64>, 12> Search::s_historyTable{};

// ... keep perft() and divide() implementation parameters unchanged ...

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

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept {
    m_nodes++;

    int originalAlpha = alpha;
    Move ttMove;
    int ttScore = 0;
    int ttDepth = 0;
    TTNodeType ttType;

    if (s_tt.probe(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType, alpha, beta)) {
        if (ttDepth >= depth) {
            return ttScore;
        }
    }

    if (depth == 0) return evaluate(pos);

    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);

    if (legalMoves.size() == 0) {
        if (MoveGenerator::inCheck(pos, pos.getSideToMove())) {
            return -MATE + ply; 
        }
        return 0; 
    }

    // Phase AA Separation: Hand the move list to the orderer before executing the search loop
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
        if (score > alpha) {
            alpha = score;
        }
        
        // Beta Cutoff hit: This move is too good, opponent will avoid this branch entirely
        if (alpha >= beta) {
            // Find what piece moved to determine if this was a quiet move
            Bitboard targetBit = 1ULL << static_cast<size_t>(legalMoves[i].getToSquare());
            bool isNormalCapture = (pos.getTotalOccupancy() & targetBit) != 0;
            bool isEnPassantCapture = (legalMoves[i].getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);
            bool isCaptureMove = isNormalCapture || isEnPassantCapture;

            if (!isCaptureMove && ply < 64) {
                // Store killer move slots (Shift slot 0 back to slot 1)
                if (s_killerMoves[ply][0].getRawData() != legalMoves[i].getRawData()) {
                    s_killerMoves[ply][1] = s_killerMoves[ply][0];
                    s_killerMoves[ply][0] = legalMoves[i];
                }
                
                // Accumulate history success weight by scanning for the piece type
                Piece movingPiece = Piece::None;
                for (size_t p = 0; p < 12; ++p) {
                    if (pos.getPieceBitboard(static_cast<Piece>(p)) & (1ULL << static_cast<size_t>(legalMoves[i].getFromSquare()))) {
                        movingPiece = static_cast<Piece>(p);
                        break;
                    }
                }

                if (movingPiece != Piece::None) {
                    size_t pIdx = static_cast<size_t>(movingPiece);
                    size_t toIdx = static_cast<size_t>(legalMoves[i].getToSquare());
                    s_historyTable[pIdx][toIdx] += depth * depth; // Weight cutoff by depth impact
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

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    int score = 0;
    m_nodes = 0;
    s_tt.clear();
    
    // Reset our dynamic search heuristic tables
    for (auto& row : s_killerMoves) row.fill(Move());
    for (auto& row : s_historyTable) row.fill(0);

    std::cout << "[BOSON SEARCH] Running Ordered Alpha-Beta Deepening Core...\n";
    
    for (int d = 1; d <= maxDepth; ++d) {
        score = negamax(pos, d, -INF, INF, 0);
        std::cout << "  -> Depth " << d << " Complete. Score: " << score 
                  << " | Total Node Visits: " << m_nodes << "\n";
    }

    std::cout << "\n--- Transposition Cache Statistics ---\n";
    std::cout << "  -> Total Probes       : " << s_tt.getProbes() << "\n";
    std::cout << "  -> Cache Hits         : " << s_tt.getHits() << "\n";
    std::cout << "  -> Cache Cutoffs      : " << s_tt.getCutoffs() << "\n";
    
    return score;
}

} // namespace Boson