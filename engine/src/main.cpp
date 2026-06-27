#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "board/MoveExecutor.hpp"
#include "search/Search.hpp" // Ensure search header is accessible

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 5 Optimization Framework Verification\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    // 1. Verify Hash Integrity Core Symmetries
    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    uint64_t baselineHash = pos->getHashKey();
    std::cout << "Initial Position Zobrist Hash : " << std::hex << baselineHash << std::dec << "\n";

    Boson::Move openingMove(Boson::Square::E2, Boson::Square::E4, Boson::Move::Flags::DoublePawnPush);
    Boson::UndoState undo;

    std::cout << "Executing e2e4 step...\n";
    Boson::MoveExecutor::makeMove(*pos, openingMove, undo);
    
    std::cout << "Undoing e2e4 step...\n";
    Boson::MoveExecutor::undoMove(*pos, openingMove, undo);
    uint64_t restoredHash = pos->getHashKey();

    if (baselineHash != restoredHash) {
        std::cerr << "CRITICAL FAILURE: Incremental Zobrist tracking exhibits drift!\n";
        return 1;
    }
    std::cout << "[BOSON TEST] STATUS: INCREMENTAL ZOBRIST ENGINE PASSED 100% CYCLE VERIFICATION.\n\n";

    // 2. Launch Optimized Tree Exploration Lookaheads (Phase Y Validation)
    // We will search to Depth 4 to let the transposition table fill and show cutoffs
    Boson::Search::runSearch(*pos, 4);

    return 0;
}