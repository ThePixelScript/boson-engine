#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 5 Optimization Framework Verification\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    // Load initial benchmark configuration
    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    uint64_t baselineHash = pos->getHashKey();
    std::cout << "Initial Position Zobrist Hash : " << std::hex << baselineHash << std::dec << "\n";

    // Simulate an opening push: e2-e4
    Boson::Move openingMove(Boson::Square::E2, Boson::Square::E4, Boson::Move::Flags::DoublePawnPush);
    Boson::UndoState undo;

    std::cout << "Executing e2e4 step...";
    Boson::MoveExecutor::makeMove(*pos, openingMove, undo);
    uint64_t modifiedHash = pos->getHashKey();
    std::cout << "\nPost-Move Zobrist Hash        : " << std::hex << modifiedHash << std::dec << "\n";

    std::cout << "Undoing e2e4 step...";
    Boson::MoveExecutor::undoMove(*pos, openingMove, undo);
    uint64_t restoredHash = pos->getHashKey();
    std::cout << "\nRestored Zobrist Hash         : " << std::hex << restoredHash << std::dec << "\n";

    if (baselineHash != restoredHash) {
        std::cerr << "CRITICAL FAILURE: Incremental Zobrist tracking exhibits drift!\n";
        return 1;
    }

    std::cout << "\n[BOSON TEST] STATUS: INCREMENTAL ZOBRIST ENGINE PASSED 100% CYCLE VERIFICATION.\n";
    return 0;
}