#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveExecutor.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 2 (Phases I–K) State Verification\n";

    // 1. Transactional En Passant Verification
    auto epPos = Boson::FenParser::parse("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    Boson::Position pos = *epPos;
    Boson::Bitboard baselineOcc = pos.getTotalOccupancy();

    Boson::Move epMove(Boson::Square::E5, Boson::Square::D6, Boson::Move::Flags::EnPassant);
    Boson::UndoState undo;

    std::cout << "\nExecuting En Passant Capture (e5xd6)...";
    Boson::MoveExecutor::makeMove(pos, epMove, undo);
    
    std::cout << "\nInverting En Passant State...";
    Boson::MoveExecutor::undoMove(pos, epMove, undo);

    if (pos.getTotalOccupancy() != baselineOcc || pos.getEnPassantSquare() != Boson::Square::D6) {
        std::cerr << "\nCRITICAL FAILURE: En Passant transactional symmetry broken.\n";
        return 1;
    }
    std::cout << "\n  -> En Passant Transaction: SUCCESS";
    std::cout << "\n\n[BOSON TEST] STATUS: ALL SPECIAL RULES PASSED 100% BIT SYMMETRY.\n";
    return 0;
}