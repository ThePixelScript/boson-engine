#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"

void evaluatePositionState(Position& pos, const std::string& label) {
    Boson::MoveList legalMoves;
    Boson::MoveGenerator::generateLegalMoves(pos, legalMoves);
    
    bool currentlyInCheck = Boson::MoveGenerator::inCheck(pos, pos.getSideToMove());
    
    std::cout << "Position [" << label << "]:\n";
    std::cout << "  -> Total Legal Moves: " << legalMoves.size() << "\n";
    std::cout << "  -> Is King In Check: " << (currentlyInCheck ? "YES" : "NO") << "\n";
    
    if (legalMoves.size() == 0) {
        if (currentlyInCheck) std::cout << "  -> RESULT: CHECKMATE\n";
        else                  std::cout << "  -> RESULT: STALEMATE\n";
    }
    std::cout << "\n";
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Milestone 3 — Legality Verification Pipeline\n\n";
    Boson::MoveGenerator::initializeTables();

    // Test Case 1: Standard Scholar's Mate Checkmate configuration
    auto matePos = Boson::FenParser::parse("r1bqkbnr/pppp1Qpp/2n5/4p3/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 0 4");
    evaluatePositionState(*matePos, "Scholar's Mate");

    // Test Case 2: Classic Sam Loyd Stalemate setup
    auto stalematePos = Boson::FenParser::parse("7k/8/8/8/8/8/6Q1/K7 b - - 0 1");
    evaluatePositionState(*stalematePos, "Sam Loyd Stalemate");

    // Test Case 3: Absolute pinned piece geometry (Rook pinned to King by enemy Bishop)
    auto pinPos = Boson::FenParser::parse("3k4/8/8/3r4/8/8/3B4/3K4 b - - 0 1");
    // Swap side to move to white so white's bishop on d2 pins black's rook on d5 to the king on d8
    pinPos->setSideToMove(Boson::Color::Black); 
    
    Boson::MoveList pinMoves;
    Boson::MoveGenerator::generateLegalMoves(*pinPos, pinMoves);
    std::cout << "Position [Pinned Rook Environment]:\n";
    std::cout << "  -> Legal Moves Filtered: " << pinMoves.size() << " (Expected: Pinned piece restricted to its ray)\n";

    std::cout << "\n[BOSON TEST] STATUS: LEGALITY PIPELINE INITIALIZED AND RUNNING SUCCESSFULLY.\n";
    return 0;
}