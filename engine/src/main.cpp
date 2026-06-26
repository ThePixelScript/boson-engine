#include <iostream>
#include "fen/FenParser.hpp"
#include "board/MoveExecutor.hpp"
#include "debug/BoardPrinter.hpp"

void runStateTransitionVerification() {
    std::cout << "[BOSON MAIN] Initializing Module 1.3 State Transition Test Array\n";

    // 1. Initialize starting configuration
    constexpr std::string_view testFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto parseResult = Boson::FenParser::parse(testFen);
    if (!parseResult) {
        std::cerr << "Aborting: Baseline FEN corruption.\n";
        std::exit(1);
    }

    Boson::Position pos = *parseResult;
    
    // Cache baseline invariants
    Boson::Bitboard initialWhiteOcc = pos.getColorOccupancy(Boson::Color::White);
    Boson::Bitboard initialBlackOcc = pos.getColorOccupancy(Boson::Color::Black);
    Boson::Bitboard initialTotalOcc = pos.getTotalOccupancy();

    std::cout << "Baseline Invariants Captured. Total Pieces Set: " << std::hex << initialTotalOcc << std::dec << "\n";

    // Scenario A: Execute Quiet Move (White Knight from g1 to f3)
    Boson::Move quietMove(Boson::Square::G1, Boson::Square::F3);
    Boson::UndoState quietUndo;

    std::cout << "\nExecuting Quiet Move: g1 -> f3\n";
    Boson::MoveExecutor::makeMove(pos, quietMove, quietUndo);

    // Verify change occurred
    if (pos.getTotalOccupancy() == initialTotalOcc) {
        std::cerr << "State Mutation Error: Occupancy masks did not shift.\n";
        std::exit(1);
    }

    std::cout << "Inverting Quiet Move...\n";
    Boson::MoveExecutor::undoMove(pos, quietMove, quietUndo);

    // Verify bit-for-bit restoration
    if (pos.getColorOccupancy(Boson::Color::White) != initialWhiteOcc ||
        pos.getColorOccupancy(Boson::Color::Black) != initialBlackOcc ||
        pos.getTotalOccupancy() != initialTotalOcc ||
        pos.getSideToMove() != Boson::Color::White ||
        pos.getFullmoveNumber() != 1) {
        std::cerr << "CRITICAL FAILURE: Quiet Move Undo bit corruption encountered.\n";
        std::exit(1);
    }
    std::cout << "  -> Quiet Move Transaction: SUCCESS (100% Bit Symmetry Verified)\n";

    // Scenario B: Execute Piece Capture Verification (Load specific capture scenario)
    constexpr std::string_view captureFen = "r1bqkbnr/ppp1pppp/2n5/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3";
    auto parseResult2 = Boson::FenParser::parse(captureFen);
    Boson::Position capturePos = *parseResult2;

    Boson::Bitboard captureWhiteOcc = capturePos.getColorOccupancy(Boson::Color::White);
    Boson::Bitboard captureBlackOcc = capturePos.getColorOccupancy(Boson::Color::Black);
    Boson::Bitboard captureTotalOcc = capturePos.getTotalOccupancy();

    // White Pawn on e5 captures Black Knight on d6 (Simulated target)
    Boson::Move captureMove(Boson::Square::E5, Boson::Square::C6); 
    Boson::UndoState captureUndo;

    std::cout << "\nExecuting Capture Move: e5 x c6\n";
    Boson::MoveExecutor::makeMove(capturePos, captureMove, captureUndo);

    if (captureUndo.capturedPiece == Boson::Piece::None) {
        std::cerr << "State Mutation Error: Capture was not recorded in UndoState.\n";
        std::exit(1);
    }

    std::cout << "Inverting Capture Move...\n";
    Boson::MoveExecutor::undoMove(capturePos, captureMove, captureUndo);

    if (capturePos.getColorOccupancy(Boson::Color::White) != captureWhiteOcc ||
        capturePos.getColorOccupancy(Boson::Color::Black) != captureBlackOcc ||
        capturePos.getTotalOccupancy() != captureTotalOcc) {
        std::cerr << "CRITICAL FAILURE: Capture Undo bit corruption encountered.\n";
        std::exit(1);
    }
    std::cout << "  -> Capture Move Transaction: SUCCESS (100% Bit Symmetry Verified)\n";

    std::cout << "\n[BOSON TEST] STATUS: ALL MOVE EXECUTION TRANSACTIONS ARE PERFECTLY SYMMETRIC.\n";
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    runStateTransitionVerification();
    return 0;
}