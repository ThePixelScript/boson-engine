#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Phase B & C Verification Pipeline\n";
    Boson::MoveGenerator::initializeTables();

    // 1. Verify King Mechanics (Blocked by friendly pawns, captures enemy knight on b1)
    // Rank 1: Kn6 (King on a1, Black Knight on b1, 6 empty)
    // Rank 2: PP6 (White Pawns on a2, b2)
    auto kingPos = Boson::FenParser::parse("8/8/8/8/8/8/PP6/Kn6 w - - 0 1");
    Boson::MoveList kingMoves;
    Boson::MoveGenerator::generateKingMoves(*kingPos, kingMoves);
    
    std::cout << "\nKing Verification (Corner a1 block/capture layout):\n";
    std::cout << "  -> Generated Moves: " << kingMoves.size() << " (Expected: 1 - a1xb1 capture, while a2 and b2 are blocked)\n";
    if (kingMoves.size() != 1) {
        std::cerr << "CRITICAL FAILURE: King geometry or blocking calculation error.\n";
        return 1;
    }

    // 2. Verify Pawn pushes and double push constraints
    auto pawnPushPos = Boson::FenParser::parse("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    Boson::MoveList pawnMoves;
    Boson::MoveGenerator::generatePawnMoves(*pawnPushPos, pawnMoves);
    
    std::cout << "\nPawn Move Verification (Standard opening layout options):\n";
    std::cout << "  -> Total Pawn Moves Found: " << pawnMoves.size() << " (Expected: 16 - e4 pawn can capture d5, others push)\n";
    if (pawnMoves.size() != 16) {
        std::cerr << "CRITICAL FAILURE: Pawn push/capture matrix count mismatch.\n";
        return 1;
    }
    
    std::cout << "\n[BOSON TEST] STATUS: ALL LEAPER AND PAWN GEOMETRIES ARE PASSED PERFECTLY.\n";
    return 0;
}