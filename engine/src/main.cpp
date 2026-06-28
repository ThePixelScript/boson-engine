#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/SearchLimits.hpp"
#include "search/SearchController.hpp"
#include "search/LMRPolicy.hpp"
#include "search/Search.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 6 — Counter-Move History (CMH)\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();
    Boson::LMRPolicy::initializeTable();

    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    Boson::SearchLimits limits;
    limits.wtime = 2000;
    limits.winc = 50;
    limits.depth = 6; 

    auto& controller = Boson::SearchController::getInstance();
    controller.initSearch(limits, *pos);

    Boson::Search::runSearch(*pos, limits.depth);
    std::cout << "[BOSON MAIN] Running Milestone 6 — Continuation History (CONTHIST)\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();
    Boson::LMRPolicy::initializeTable();
    // Initialize the static table instances
    const_cast<Boson::ContinuationHistoryTable&>(Boson::Search::getContHist()).clear();
    
    return 0;
}