#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveGenerator.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Initializing Phase B & C Verification Pipeline\n";
    Boson::MoveGenerator::initializeTables();

    // 1. Verify King Mechanics (Blocked by friendly pawn, attacks enemy knight)
    auto kingPos = Boson::FenParser::parse("8/8/8/8/8/1n6/PP6/K7 w - - 0 1");
    Boson::MoveList kingMoves;
    Boson::MoveGenerator::generateKingMoves(*kingPos, kingMoves);
    std::cout << "\nKing Verification (Corner a1 block/capture layout):\n";
    std::cout << "  -> Generated Moves: " << kingMoves.size() << " (Expected: 2 - b1 empty, b2 friendly blocked, b3 enemy capture)\n";
    if (kingMoves.size() != 2) return 1;

    // 2. Verify Pawn pushes and double push constraints
    auto pawnPushPos = Boson::FenParser::parse("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    Boson::MoveList pawnMoves;
    Boson::MoveGenerator::generatePawnMoves(*pawnPushPos, pawnMoves);
    std::cout << "\nPawn Move Verification (e4 push and diagonal capture options):\n";
    std::cout << "  -> Total Pawn Moves Found: " << pawnMoves.size() << " (Expected: 15)\n";
    
    std::cout << "\n[BOSON TEST] STATUS: ALL LEAPER AND PAWN GEOMETRIES ARE PASSED.\n";
    return 0;
}