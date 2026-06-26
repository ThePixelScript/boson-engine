#include "search/Search.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include <iostream>

namespace Boson {

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

        // Convert square indices back to coordinate notation for quick terminal debugging
        int from = static_cast<int>(m.getFromSquare());
        int to = static_cast<int>(m.getToSquare());
        char fFile = 'a' + (from % 8);
        char fRank = '1' + (from / 8);
        char tFile = 'a' + (to % 8);
        char tRank = '1' + (to / 8);
        
        std::cout << fFile << fRank << tFile << tRank << " : " << nodesForMove << "\n";
    }
    std::cout << "Total Nodes: " << totalNodes << "\n-------------------------\n";
}

int Search::evaluate(const Position& pos) noexcept {
    // Phase T Leaf Node: Strict material valuation baseline
    int score = 0;
    
    // Standard piece weight fields
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

    // Flip evaluation perspective relative to active side to move (Canonical Negamax)
    return (pos.getSideToMove() == Color::White) ? score : -score;
}

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept {
    if (depth == 0) return evaluate(pos);

    MoveList legalMoves;
    MoveGenerator::generateLegalMoves(pos, legalMoves);

    // Terminal Evaluation: Checkmate or Stalemate
    if (legalMoves.size() == 0) {
        if (MoveGenerator::inCheck(pos, pos.getSideToMove())) {
            return -MATE + ply; // Favor shorter mate paths
        }
        return 0; // Stalemate
    }

    int bestScore = -INF;

    for (size_t i = 0; i < legalMoves.size(); ++i) {
        UndoState undo;
        MoveExecutor::makeMove(pos, legalMoves[i], undo);
        
        // Phase T Canonical Inversion Layer
        int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1);
        
        MoveExecutor::undoMove(pos, legalMoves[i], undo);

        if (score > bestScore) bestScore = score;
        if (score > alpha)        alpha = score;
        if (alpha >= beta)        break; // Phase U: Alpha-Beta Cutoff
    }
    return bestScore;
}

int Search::runSearch(Position& pos, int maxDepth) noexcept {
    int score = 0;
    std::cout << "[BOSON SEARCH] Launching Iterative Deepening Engine Layer...\n";
    
    // Phase V: Loop to target depths sequentially to maximize ordering stability
    for (int d = 1; d <= maxDepth; ++d) {
        score = negamax(pos, d, -INF, INF, 0);
        std::cout << "  -> Depth " << d << " Complete. Best Evaluation Score: " << score << "\n";
    }
    return score;
}

} // namespace Boson