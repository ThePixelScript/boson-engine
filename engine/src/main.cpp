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

    std::cout << "[BOSON MAIN] Running Milestone 6 — Null Move Pruning Integration\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // Setup standard 2000ms base pool with a 50ms increment per move
    Boson::SearchLimits limits;
    limits.wtime = 2000;
    limits.winc = 50;
    limits.depth = 6; // Profile up to Depth 6

    auto& controller = Boson::SearchController::getInstance();
    controller.initSearch(limits, *pos);

    std::cout << "[BOSON] Soft Search Budget Allocated: " << controller.getTimeManager().getSoftLimit() << "ms\n";
    std::cout << "[BOSON] Hard Search Budget Cutoff:    " << controller.getTimeManager().getHardLimit() << "ms\n\n";

    // Launch the core search driver
    Boson::Search::runSearch(*pos, limits.depth);

    return 0;
}