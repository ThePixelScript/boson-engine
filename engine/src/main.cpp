#include <iostream>
#include "fen/FenParser.hpp"
#include "search/Zobrist.hpp"
#include "board/MoveGenerator.hpp"
#include "evaluation/Evaluator.hpp"
#include "search/Search.hpp"

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "[BOSON MAIN] Running Milestone 5 — Phase Z Evaluation Checks\n";
    Boson::Zobrist::initialize();
    Boson::MoveGenerator::initializeTables();

    // Test Case 1: Initial Position Symmetry
    auto normalPos = Boson::FenParser::parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int initialScore = Boson::Evaluator::evaluate(*normalPos);
    std::cout << "  -> Initial Position Score: " << initialScore << " (Expected: 0 due to symmetric layout)\n";

    // Test Case 2: Positional preference verification (Knight to Center)
    auto cornerKnight = Boson::FenParser::parse("r1bqkbnr/pppppppp/8/8/8/n7/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto centerKnight = Boson::FenParser::parse("r1bqkbnr/pppppppp/8/3n4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    // Evaluate from White's perspective but look at Black's piece changes
    int scoreCorner = Boson::Evaluator::evaluate(*cornerKnight);
    int scoreCenter = Boson::Evaluator::evaluate(*centerKnight);
    
    std::cout << "  -> Black Knight on Edge (A3) score perspective: " << scoreCorner << "\n";
    std::cout << "  -> Black Knight in Center (D5) score perspective: " << scoreCenter << "\n";
    
    if (scoreCenter < scoreCorner) {
        std::cout << "  -> POSITIONAL ADVANTAGE CHECK: PASSED (Center development penalizes White's side)\n";
    } else {
        std::cerr << "  -> CRITICAL METRIC FAILURE: PST bonuses are inverted!\n";
        return 1;
    }

    // Test Case 3: Pure Mirror Symmetry Verification
    auto whiteAdvantage = Boson::FenParser::parse("k7/8/8/8/8/8/P7/K7 w - - 0 1");
    auto blackAdvantage = Boson::FenParser::parse("k7/p7/8/8/8/8/8/K7 b - - 0 1");

    int scoreWhite = Boson::Evaluator::evaluate(*whiteAdvantage);
    int scoreBlack = Boson::Evaluator::evaluate(*blackAdvantage);

    std::cout << "  -> White Extra Pawn Score : " << scoreWhite << "\n";
    std::cout << "  -> Mirrored Black Extra Pawn Score: " << scoreBlack << "\n";

    if (scoreWhite == scoreBlack) {
        std::cout << "  -> MIRROR SYMMETRY CONTRACT: VALIDATED 100% SACCADIC MATCH.\n";
    } else {
        std::cerr << "  -> CRITICAL METRIC FAILURE: Symmetrical structures mismatch!\n";
        return 1;
    }

    // Launch full search lookahead using the positional evaluator
    Boson::Search::runSearch(*normalPos, 4);

    return 0;
}