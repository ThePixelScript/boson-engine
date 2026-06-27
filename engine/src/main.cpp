#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/see/SEE.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 6 — Module 6.5 SEE Tactical Diagnostics\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    // =========================================================================
    // TEST CASE 1: Profitable Trade (The Classical Open-File Exchange)
    // Position FEN: White Rook on d1, White Rook on d2. Black Rook on d8.
    // White d2 Rook captures Black d8 Rook. 
    // Sequence: RxR (+500), RxR (-500), RxR (+500). Net Result = +500 (Gain a Rook)
    // =========================================================================
    auto profitableFen = Boson::FenParser::parse("3r4/8/8/8/8/8/3R4/3R4 w - - 0 1");
    int scoreGood = Boson::SEE::evaluate(*profitableFen, Boson::Square::D2, Boson::Square::D8);
    std::cout << "  -> Open File Rook Trade SEE Value: " << scoreGood << " (Expected: 500)\n";

    // =========================================================================
    // TEST CASE 2: Hanging Queen Trap
    // White Queen on d1 captures Black Pawn on d5. Black Knight on c6 protects d5.
    // Sequence: QxP (+100), NxQ (-900). Net Result = -800
    // =========================================================================
    auto trapFen = Boson::FenParser::parse("r1bqkbnr/ppp1pppp/2n5/3p4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int scoreBad = Boson::SEE::evaluate(*trapFen, Boson::Square::D1, Boson::Square::D5);
    std::cout << "  -> Hanging Queen Trap SEE Value:   " << scoreBad << " (Expected: -800)\n";

    if (scoreGood == 500 && scoreBad == -800) {
        std::cout << "\n[BOSON TEST] STATUS: MODULE 6.5 SEE SIMULATOR PASSED TACTICAL VALIDATION.\n";
    } else {
        std::cerr << "\n[BOSON TEST] CRITICAL FAILURE: SEE calculations are out of alignment!\n";
        return 1;
    }

    return 0;
}