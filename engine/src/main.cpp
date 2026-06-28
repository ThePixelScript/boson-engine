#include <iostream>
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/LMRPolicy.hpp"
#include "search/Search.hpp"
#include "evaluation/Evaluator.hpp"
#include "validation/VerificationHarness.hpp"
// Include headers needed for our isolated diagnostic tools
#include "fen/FenParser.hpp"
#include "board/MoveExecutor.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "🏛️  BOSON — MILESTONE Ω: HARNESS GATEWAY\n";
    
    // Global Subsystem Initialization
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();
    Boson::LMRPolicy::initializeTable();
    
    // Clear transient tables between verification states
    const_cast<Boson::ContinuationHistoryTable&>(Boson::Search::getContHist()).clear();
    Boson::Evaluator::getCorrHist().clear();

    // =========================================================================
    // TARGETED DIAGNOSTIC: Force-inspecting the b4f4 after-effects
    // =========================================================================
    std::cout << "\n🔎 RUNNING ISOLATED b4f4 SUB-TREE AUDIT...\n";
    
    // 1. Parse CPW Position
    auto pos = Boson::FenParser::parse("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    
    if (pos) {
        // 2. Explicitly execute b4f4 (White Rook captures f4 Pawn)
        Boson::Move rxf4(Boson::Square::B4, Boson::Square::F4);
        Boson::UndoState undo;
        Boson::MoveExecutor::makeMove(*pos, rxf4, undo);
        
        // 3. Run divide on the resulting position from Black's perspective at Depth 3
        Boson::Search::divide(*pos, 3);
    }
    std::cout << "=========================================================\n\n";
    // =========================================================================

    // Call the newly aligned multi-FEN harness suite
    bool perftPassed = Boson::VerificationHarness::runComprehensivePerft();
    if (!perftPassed) {
        std::cerr << "\n[BOSON AUDIT] TERMINATED: Node anomalies detected during perft verification!\n";
        return 1;
    }

    // Execute monolithic search engine benchmarks
    Boson::VerificationHarness::executePerformanceBenchmark();

    std::cout << "\n[BOSON AUDIT] STATUS: ALL SYSTEMS OPERATIONAL.\n";
    return 0;
}