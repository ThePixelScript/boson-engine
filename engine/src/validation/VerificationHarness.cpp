#include "validation/VerificationHarness.hpp"
#include "fen/FenParser.hpp"
#include "search/Search.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

namespace Boson {

const std::vector<PerftTestCase> VerificationHarness::S_PERFT_SUITE = {}; 

bool VerificationHarness::runComprehensivePerft() noexcept {
    std::cout << "\n=================================================================\n";
    std::cout << "🏛️  BOSON MODULE Ω.1 — RUNNING REALITY STRESS PERFT MATRIX\n";
    std::cout << "=================================================================\n";
    
    struct LocalTestCase {
        std::string fenString;
        int targetDepth;
        uint64_t nodesExpected;
        std::string testLabel;
    };

    const std::vector<LocalTestCase> realitySuite = {
        { "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 3, 9483ULL, "Pos 6: Wagner Promo Stress" },
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL, "Pos 7: Multi-Check Evasions" },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 2", 3, 9467ULL, "Pos 8: Anarchist Evasions" },
        { "8/k7/3p4/p7/Pp1pPp2/1P1P4/6P1/1K6 b - - 0 1", 4, 1222ULL, "Pos 9: Blocked Pawn Endgame" },
        { "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 3, 9467ULL, "Pos 10: Tricky Middlegame" }
    };

    bool allPassed = true;
    
    for (const auto& test : realitySuite) {
        auto pos = FenParser::parse(test.fenString);
        std::cout << "  -> Testing: " << std::left << std::setw(32) << test.testLabel 
                  << " (Depth " << test.targetDepth << ")... ";
        
        uint64_t actualNodes = Search::perft(*pos, test.targetDepth);
        
        if (actualNodes == test.nodesExpected) {
            std::cout << "✅ PASSED\n";
        } else {
            std::cout << "❌ MISMATCH!\n"
                      << "     Expected: " << test.nodesExpected << "\n"
                      << "     Actual:   " << actualNodes << "\n";
            allPassed = false;
        }
    }
    return allPassed;
}

void VerificationHarness::executePerformanceBenchmark() noexcept {}

} // namespace Boson