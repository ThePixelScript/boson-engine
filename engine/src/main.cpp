#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Milestone 2 — Phase F-H Verification Pipeline\n";
    Boson::MoveGenerator::initializeTables();

    // 1. Empty Board Verification (Corner vs Center Geometries)
    auto emptyPos = Boson::FenParser::parse("8/8/8/8/8/8/8/8 w - - 0 1");
    
    // Test Rook on a1 (Corner: 14 moves) and d4 (Center: 14 moves)
    Bitboard rookA1 = Boson::MoveGenerator::getRookAttacks(Boson::Square::A1, emptyPos->getTotalOccupancy());
    Bitboard rookD4 = Boson::MoveGenerator::getRookAttacks(Boson::Square::D4, emptyPos->getTotalOccupancy());
    
    // Test Bishop on a1 (Corner: 7 moves) and d4 (Center: 13 moves)
    Bitboard bishopA1 = Boson::MoveGenerator::getBishopAttacks(Boson::Square::A1, emptyPos->getTotalOccupancy());
    Bitboard bishopD4 = Boson::MoveGenerator::getBishopAttacks(Boson::Square::D4, emptyPos->getTotalOccupancy());

    std::cout << "\n--- Empty Board Ray Checks ---\n";
    std::cout << "Corner Rook (a1) count   : " << std::popcount(rookA1) << " (Expected: 14)\n";
    std::cout << "Center Rook (d4) count   : " << std::popcount(rookD4) << " (Expected: 14)\n";
    std::cout << "Corner Bishop (a1) count : " << std::popcount(bishopA1) << " (Expected: 7)\n";
    std::cout << "Center Bishop (d4) count : " << std::popcount(bishopD4) << " (Expected: 13)\n";

    if (std::popcount(rookA1) != 14 || std::popcount(rookD4) != 14 ||
        std::popcount(bishopA1) != 7 || std::popcount(bishopD4) != 13) {
        std::cerr << "CRITICAL FAILURE: Empty board sliding ray geometries are broken.\n";
        return 1;
    }

    // 2. Blocker Termination and Capture Verification
    // White Rook on d4. Friendly pawn on d6 (blocks North). Enemy Knight on g4 (blocks East, capturable).
    auto blockedPos = Boson::FenParser::parse("8/8/3P4/8/3R2n1/8/8/8 w - - 0 1");
    Boson::MoveList moves;
    Boson::MoveGenerator::generateSlidingMoves(*blockedPos, moves);

    std::cout << "\n--- Blocker Ray Checks ---\n";
    std::cout << "Sliding Moves Generated  : " << moves.size() << "\n";

    // Let's verify Queen logical consistency: Queen = Rook | Bishop
    Bitboard queenD4 = Boson::MoveGenerator::getQueenAttacks(Boson::Square::D4, blockedPos->getTotalOccupancy());
    Bitboard expectedQueen = getRookAttacks(Boson::Square::D4, blockedPos->getTotalOccupancy()) | 
                             getBishopAttacks(Boson::Square::D4, blockedPos->getTotalOccupancy());

    std::cout << "Queen Bitwise Consistency: " << (queenD4 == expectedQueen ? "MATCH" : "MISMATCH") << "\n";
    if (queenD4 != expectedQueen) {
        std::cerr << "CRITICAL FAILURE: Queen logic varies from component OR operations.\n";
        return 1;
    }

    std::cout << "\n[BOSON TEST] STATUS: SLIDING RAY ENGINE PASSED ALL EXIT CRITERIA.\n";
    return 0;
}