#include <iostream>
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/LMRPolicy.hpp"
#include "search/Search.hpp"
#include "evaluation/Evaluator.hpp"
#include "validation/VerificationHarness.hpp"

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