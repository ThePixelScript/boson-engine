#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "search/SearchLimits.hpp"
#include "search/SearchController.hpp"
#include "search/LMRPolicy.hpp"
#include "search/Search.hpp"
#include "evaluation/Evaluator.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 6 — Correction History (CORRHIST)\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();
    Boson::LMRPolicy::initializeTable();
    
    const_cast<Boson::ContinuationHistoryTable&>(Boson::Search::getContHist()).clear();
    Boson::Evaluator::getCorrHist().clear(); // Reset the correction layers

    Boson::SearchLimits limits;
    limits.wtime = 2000;
    limits.winc = 50;
    limits.depth = 6; 

    auto pos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto& controller = Boson::SearchController::getInstance();
    controller.initSearch(limits, *pos);

    Boson::Search::runSearch(*pos, limits.depth);

    return 0;
}