#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/SearchLimits.hpp"
#include "search/SearchController.hpp"
#include "search/Search.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 6 — Time Management Pipeline\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // Simulate standard clock: 2000ms left, 50ms increment. Max depth allowance 10 plies.
    Boson::SearchLimits limits;
    limits.wtime = 2000;
    limits.winc = 50;
    limits.depth = 10;

    // Load limits into the Search controller
    auto& controller = Boson::SearchController::getInstance();
    controller.initSearch(limits, *pos);

    std::cout << "[BOSON] Soft Budget Allocated: " << controller.getTimeManager().getSoftLimit() << "ms\n";
    std::cout << "[BOSON] Hard Budget Cutoff:    " << controller.getTimeManager().getHardLimit() << "ms\n\n";

    // Run iterative deepening loop
    Boson::Search::runSearch(*pos, limits.depth);

    return 0;
}