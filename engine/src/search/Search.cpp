#include "search/Search.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>
#include <bit>

namespace Boson {

TranspositionTable Search::s_tt(16); // 16 Megabytes search cache
uint64_t Search::m_nodes = 0;

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
    int score = 0;
    score += std::popcount(pos.getPieceBitboard(Piece::WhitePawn)) * 100;
    score -= std::popcount(pos.getPieceBitboard(Piece::BlackPawn)) * 100;
    score += std::popcount(pos.getPieceBitboard(Piece::WhiteKnight)) * 320;
    score -= std::popcount(pos.getPieceBitboard(Piece::BlackKnight)) * 320;
    score += std::popcount(pos.getPieceBitboard(Piece::WhiteBishop)) * 330;
    score -= std::popcount(pos.getPieceBitboard(Piece::BlackBishop)) * 330;
    score += std::popcount(pos.getPieceBitboard(Piece::WhiteRook)) * 500;
    score -= std::popcount(pos.getPieceBitboard(Piece::BlackRook)) * 500;
    score += std::popcount(pos.getPieceBitboard(Piece::WhiteQueen)) * 900;
    score -= std::popcount(pos.getPieceBitboard(Piece::BlackQueen)) * 900;

    return (pos.getSideToMove() == Color::White) ? score : -score;
}

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept {
    m_nodes++;

    int originalAlpha = alpha;
    Move ttMove;
    int ttScore = 0;
    int ttDepth = 0; // FIXED: Initialize to 0, let s_tt.probe fill the real depth!
    TTNodeType ttType;

    // Phase Y Cache Probe (Passing the actual current required depth as an explicit constraint validation check)
    if (s_tt.probe(pos.getHashKey(), ttScore, ttMove, ttDepth, ttType, alpha, beta)) {
        // Double check depth verification inside the probe framework matching 'depth'
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

    int bestScore = -INF;
    Move bestMove;

    // 1. High-Priority: Search the TT Move first if it exists and is valid
    bool searchedTTMove = false;
    if (ttMove.getRawData() != 0) {
        for (size_t i = 0; i < legalMoves.size(); ++i) {
            if (legalMoves[i].getRawData() == ttMove.getRawData()) {
                UndoState undo;
                MoveExecutor::makeMove(pos, legalMoves[i], undo);
                int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
                MoveExecutor::undoMove(pos, legalMoves[i], undo);

                if (score > bestScore) {
                    bestScore = score;
                    bestMove = legalMoves[i];
                }
                if (score > alpha) alpha = score;
                searchedTTMove = true;
                break;
            }
        }
        if (alpha >= beta) goto save_node;
    }

    // 2. Search all other remaining moves
    for (size_t i = 0; i < legalMoves.size(); ++i) {
        if (searchedTTMove && legalMoves[i].getRawData() == ttMove.getRawData()) {
            continue;
        }

        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        
        int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
        
        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (score > bestScore) {
            bestScore = score;
            bestMove = legalMoves[i];
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break; 
    }

save_node:
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
    std::cout << "[BOSON SEARCH] Running Optimized Iterative Deepening Pipeline...\n";
    
    for (int d = 1; d <= maxDepth; ++d) {
        score = negamax(pos, d, -INF, INF, 0);
        std::cout << "  -> Depth " << d << " Complete. Score: " << score 
                  << " | Total Node Visits: " << m_nodes << "\n";
    }

    std::cout << "\n--- Transposition Cache Statistics ---\n";
    std::cout << "  -> Total Probes       : " << s_tt.getProbes() << "\n";
    std::cout << "  -> Cache Hits         : " << s_tt.getHits() << "\n";
    std::cout << "  -> Cache Cutoffs      : " << s_tt.getCutoffs() << "\n";
    std::cout << "  -> Table Collisions   : " << s_tt.getCollisions() << "\n";
    
    return score;
}

} // namespace Boson