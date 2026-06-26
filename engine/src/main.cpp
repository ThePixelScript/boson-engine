#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Milestone 2 — Phase A (Knight Geometry Validation)\n";

    // 1. Fire table generation at engine boot up
    Boson::MoveGenerator::initializeTables();

    // Test position 1: Knight isolated on e4 (Square index 28)
    constexpr std::string_view isolatedKnightFen = "8/8/8/8/4N3/8/8/8 w - - 0 1";
    auto pos1 = Boson::FenParser::parse(isolatedKnightFen);
    
    Boson::MoveList moves1;
    Boson::MoveGenerator::generateKnightMoves(*pos1, moves1);

    std::cout << "\nScenario A: Isolated Central White Knight on e4\n";
    std::cout << "  -> Total Pseudo-Legal Moves Generated: " << moves1.size() << " (Expected: 8)\n";
    if (moves1.size() != 8) {
        std::cerr << "CRITICAL FAILURE: Center attack lookup broken.\n";
        return 1;
    }

    // Test position 2: White Knight trapped in the corner on a1 (Square index 0)
    constexpr std::string_view cornerKnightFen = "8/8/8/8/8/8/8/N7 w - - 0 1";
    auto pos2 = Boson::FenParser::parse(cornerKnightFen);
    
    Boson::MoveList moves2;
    Boson::MoveGenerator::generateKnightMoves(*pos2, moves2);

    std::cout << "\nScenario B: Trapped Corner White Knight on a1\n";
    std::cout << "  -> Total Pseudo-Legal Moves Generated: " << moves2.size() << " (Expected: 2)\n";
    if (moves2.size() != 2) {
        std::cerr << "CRITICAL FAILURE: Corner boundary truncation check failed.\n";
        return 1;
    }

    std::cout << "\n[BOSON TEST] STATUS: KNIGHT GEOMETRICAL TABLES PASSED INTRINSIC VALIDATION.\n";
    return 0;
}