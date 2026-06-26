#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"
#include "search/Search.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Milestone 4 Framework Checks\n";
    Boson::MoveGenerator::initializeTables();

    // 1. Load Initial Position Baseline
    auto initialPos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "\n--- Running Baseline Perft Tree Counts ---";
    uint64_t p1 = Boson::Search::perft(*initialPos, 1);
    uint64_t p2 = Boson::Search::perft(*initialPos, 2);
    uint64_t p3 = Boson::Search::perft(*initialPos, 3);

    std::cout << "\n  -> Perft Depth 1: " << p1 << " (Expected: 20)";
    std::cout << "\n  -> Perft Depth 2: " << p2 << " (Expected: 400)";
    std::cout << "\n  -> Perft Depth 3: " << p3 << " (Expected: 8902)\n";

    if (p1 != 20 || p2 != 400 || p3 != 8902) {
        std::cerr << "CRITICAL FAILURE: Perft verification count deviation detected.\n";
        return 1;
    }
    std::cout << "Perft Validation Matrix: SUCCESS\n";

    // 2. Fire Divide Mode Diagnostic on Depth 1 to verify root move tracking
    Boson::Search::divide(*initialPos, 1);

    // 3. Verify Alpha-Beta Search lookahead functionality
    std::cout << "\n--- Running Iterative Deepening Negamax Test ---\n";
    int finalScore = Boson::Search::runSearch(*initialPos, 3);
    std::cout << "\nFinal Balanced Engine Evaluation: " << finalScore << "\n";

    std::cout << "\n[BOSON TEST] STATUS: MILESTONE 4 ENGINE INFRASTRUCTURE PASSED ACCORDING TO SPECIFICATIONS.\n";
    return 0;
}