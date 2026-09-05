#ifndef BOSON_TACTICAL_SUITE_CPP
#define BOSON_TACTICAL_SUITE_CPP

#include "tactical/TacticalSuite.hpp"
#include "fen/FenParser.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "board/Position.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Boson {

namespace {

// Category 1: Mates
const std::vector<TacticalTestCase> s_mateTests = {
    {
        "Mate in 1 - Scholar's Mate Finish",
        "r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 4",
        {"f3f7"},
        2,
        30000,
        32000,
        true,
        1
    },
    {
        "Mate in 1 - Back-Rank Mate",
        "6k1/5ppp/8/8/8/8/8/4R1K1 w - - 0 1",
        {"e1e8"},
        2,
        30000,
        32000,
        true,
        1
    },
    {
        "Mate in 2 - King & Rook Corner Box",
        "1k6/8/1K6/8/8/8/8/2R5 w - - 0 1",
        {"c1c7", "c1c6", "c1c5", "c1c4", "c1c3", "c1c2"},
        4,
        30000,
        32000,
        true,
        3
    },
    {
        "Mate in 2 - Anastasia's Queen Sacrifice",
        "4rr1k/4N1pp/8/7Q/8/3R4/5PPP/6K1 w - - 0 1",
        {"h5h7"},
        4,
        30000,
        32000,
        true,
        3
    },
    {
        "Forced Mate in 4 - WAC.001 Heavy Strike",
        "2rr2k1/1p3ppp/3p4/p2Np3/1PP1P3/2K2P2/P3q1PP/3R3R b - - 0 1",
        {"c8c4"},
        6,
        30000,
        32000,
        true,
        7
    },
    {
        "Defensive Mate Evasion - Back-Rank Guard",
        "r4rk1/pb3ppp/8/8/8/8/PPP2qPP/R1B1R2K w - - 0 1",
        {"e1g1"},
        4,
        -1500,
        32000,
        false,
        0
    }
};

// Category 2: Core Tactics
const std::vector<TacticalTestCase> s_coreTacticsTests = {
    {
        "Core Tactics - Knight Family Fork",
        "r3k2r/ppp2ppp/2n5/3N4/8/8/PPP2PPP/R3K2R w KQkq - 0 1",
        {"d5c7"},
        4,
        200,
        32000,
        false,
        0
    },
    {
        "Core Tactics - Pawn Family Fork",
        "r2qkb1r/ppp2ppp/2n1b3/4p3/3P4/2N2N2/PPP2PPP/R1BQKB1R w KQkq - 0 1",
        {"d4d5"},
        4,
        150,
        32000,
        false,
        0
    },
    {
        "Core Tactics - Absolute Pin Exploitation",
        "4k3/8/8/4n3/3P4/8/8/4R1K1 w - - 0 1",
        {"d4e5", "e1e5"},
        4,
        250,
        32000,
        false,
        0
    },
    {
        "Core Tactics - Absolute Rook Skewer",
        "8/1q2k3/8/8/8/8/8/7R w - - 0 1",
        {"h1h7"},
        4,
        300,
        32000,
        false,
        0
    },
    {
        "Core Tactics - Discovered Attack / Royal Check",
        "3qk3/8/8/4N3/8/8/8/4R1K1 w - - 0 1",
        {"e5c6"},
        4,
        300,
        32000,
        false,
        0
    },
    {
        "Core Tactics - Back-Rank Liquidation",
        "2rr2k1/5ppp/8/8/8/3R4/5PPP/3R2K1 w - - 0 1",
        {"d3d8", "d1d8"},
        4,
        30000,
        32000,
        true,
        3
    }
};

// Category 3: Search-Specific Heuristic Edge Cases
const std::vector<TacticalTestCase> s_searchEdgeCaseTests = {
    {
        "Search Edge Cases - SEE Defended Pawn Trap Avoidance",
        "4k2r/p4ppp/2p1pn2/R2p4/8/8/PPP2PPP/4KB1R w Kk - 0 1",
        {"a5a7"},
        4,
        50,
        32000,
        false,
        0
    },
    {
        "Search Edge Cases - NMP Zugzwang Protection (Pure Pawn)",
        "8/8/4k3/4p3/4P3/4K3/8/8 w - - 0 1",
        {"e3d3"},
        4,
        0,
        32000,
        false,
        0
    },
    {
        "Search Edge Cases - Quiescence Stand-Pat & Hanging Trap",
        "r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 5",
        {"d3e4"},
        4,
        200,
        32000,
        false,
        0
    },
    {
        "Search Edge Cases - LMR Quiet Breakthrough & Re-search",
        "3r2k1/5ppp/8/8/8/8/1Q3PPP/R5K1 w - - 0 1",
        {"a1e1", "b2b6", "b2a5"},
        4,
        200,
        32000,
        false,
        0
    }
};

} // anonymous namespace

std::span<const TacticalTestCase> getMateTestCases() noexcept {
    return s_mateTests;
}

std::span<const TacticalTestCase> getCoreTacticsTestCases() noexcept {
    return s_coreTacticsTests;
}

std::span<const TacticalTestCase> getSearchEdgeCaseTestCases() noexcept {
    return s_searchEdgeCaseTests;
}

TacticalTestResult TacticalRegressionRunner::runSingleTest(const TacticalTestCase& testCase, bool verbose) {
    TacticalTestResult result;
    result.testName = std::string(testCase.name);
    result.fen = std::string(testCase.fen);
    for (const auto& m : testCase.expectedBestMoves) {
        result.expectedMoves.push_back(std::string(m));
    }
    result.depthSearched = testCase.minDepth;

    auto parsed = FenParser::parse(testCase.fen);
    if (!parsed.has_value()) {
        result.passed = false;
        result.failureReason = "Failed to parse FEN string";
        return result;
    }

    Position pos = parsed.value();

    // Optionally silence search console spew to maintain clean test output
    std::streambuf* origBuf = nullptr;
    std::ostringstream dummy;
    if (!verbose) {
        origBuf = std::cout.rdbuf(dummy.rdbuf());
    }

    int evalScore = Search::runSearch(pos, testCase.minDepth);

    if (origBuf) {
        std::cout.rdbuf(origBuf);
    }

    const auto& stats = SearchController::getInstance().getStats();
    result.score = evalScore;
    result.pvString = stats.pvString;

    // Extract best move
    std::string bestMoveStr;
    if (stats.pvLine.count > 0) {
        bestMoveStr = stats.pvLine.moves[0].toString();
    } else {
        std::stringstream ss(stats.pvString);
        ss >> bestMoveStr;
    }
    result.actualMove = bestMoveStr;

    // 1. Move verification
    bool moveMatched = false;
    for (const auto& exp : testCase.expectedBestMoves) {
        if (bestMoveStr == exp) {
            moveMatched = true;
            break;
        }
    }

    if (!moveMatched) {
        result.passed = false;
        result.failureReason = "Best move mismatch: expected one of [";
        for (size_t i = 0; i < testCase.expectedBestMoves.size(); ++i) {
            if (i > 0) result.failureReason += ", ";
            result.failureReason += testCase.expectedBestMoves[i];
        }
        result.failureReason += "], but search selected [" + bestMoveStr + "]";
        return result;
    }

    // 2. Score bounds verification
    if (evalScore < testCase.expectedScoreMin || evalScore > testCase.expectedScoreMax) {
        result.passed = false;
        result.failureReason = "Score out of bounds: eval " + std::to_string(evalScore) +
                               " not in range [" + std::to_string(testCase.expectedScoreMin) +
                               ", " + std::to_string(testCase.expectedScoreMax) + "]";
        return result;
    }

    // 3. Mate verification
    if (testCase.isMate) {
        if (evalScore < Search::MATE - 100) {
            result.passed = false;
            result.failureReason = "Expected checkmate score (>= " + std::to_string(Search::MATE - 100) +
                                   "), but search returned eval " + std::to_string(evalScore);
            return result;
        }
        if (testCase.mateInPly > 0) {
            int mateDistance = Search::MATE - evalScore;
            if (mateDistance > testCase.mateInPly + 2) {
                result.passed = false;
                result.failureReason = "Mate distance mismatch: expected mate in ~" + std::to_string(testCase.mateInPly) +
                                       " ply, got mate in " + std::to_string(mateDistance) + " ply";
                return result;
            }
        }
    }

    result.passed = true;
    return result;
}

TacticalCategoryReport TacticalRegressionRunner::runCategory(std::string_view categoryName, std::span<const TacticalTestCase> tests, bool verbose) {
    TacticalCategoryReport report;
    report.categoryName = std::string(categoryName);
    report.totalTests = static_cast<int>(tests.size());

    std::cout << "\n--- Tactical Category: " << categoryName << " (" << report.totalTests << " Positions) ---\n";

    for (const auto& tc : tests) {
        TacticalTestResult res = runSingleTest(tc, verbose);
        report.results.push_back(res);

        if (res.passed) {
            report.passedTests++;
            std::cout << "  [PASS] " << tc.name
                      << " -> " << res.actualMove << " (eval: " << res.score << " cp, depth: " << res.depthSearched << ")\n";
        } else {
            report.failedTests++;
            std::cout << "  [FAIL] " << tc.name
                      << " -> " << res.actualMove << " (eval: " << res.score << " cp, depth: " << res.depthSearched << ")\n";
            std::cout << "\n[ASSERTION FAILURE] Tactical Regression Failed!\n"
                      << "  Test Name     : " << res.testName << "\n"
                      << "  FEN           : " << res.fen << "\n"
                      << "  Expected Moves: [";
            for (size_t i = 0; i < res.expectedMoves.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << res.expectedMoves[i];
            }
            std::cout << "]\n"
                      << "  Actual Move   : " << res.actualMove << "\n"
                      << "  Depth Searched: " << res.depthSearched << "\n"
                      << "  Eval Score    : " << res.score << " cp\n"
                      << "  PV String     : " << res.pvString << "\n"
                      << "  Failure Reason: " << res.failureReason << "\n"
                      << "-----------------------------------------------------------------\n";
        }
    }

    return report;
}

bool TacticalRegressionRunner::runMilestoneOmegaPhase2TacticalTests() {
    std::cout << "\n=================================================================\n";
    std::cout << "===   MILESTONE OMEGA, MODULE OMEGA.2: TACTICAL REGRESSION   ===\n";
    std::cout << "=================================================================\n";

    auto mateReport = runCategory("Category 1: Mates & Mate Sequences", getMateTestCases(), false);
    auto tacticsReport = runCategory("Category 2: Core Tactical Motifs", getCoreTacticsTestCases(), false);
    auto edgeCaseReport = runCategory("Category 3: Search Heuristic Edge Cases", getSearchEdgeCaseTestCases(), false);

    int totalTests = mateReport.totalTests + tacticsReport.totalTests + edgeCaseReport.totalTests;
    int passedTests = mateReport.passedTests + tacticsReport.passedTests + edgeCaseReport.passedTests;
    int failedTests = mateReport.failedTests + tacticsReport.failedTests + edgeCaseReport.failedTests;

    std::cout << "\n=================================================================\n";
    std::cout << "TACTICAL REGRESSION SUMMARY:\n";
    std::cout << "  Mates & Mate Sequences       : " << mateReport.passedTests << "/" << mateReport.totalTests << " Passed\n";
    std::cout << "  Core Tactical Motifs         : " << tacticsReport.passedTests << "/" << tacticsReport.totalTests << " Passed\n";
    std::cout << "  Search Heuristic Edge Cases  : " << edgeCaseReport.passedTests << "/" << edgeCaseReport.totalTests << " Passed\n";
    std::cout << "-----------------------------------------------------------------\n";
    std::cout << "TOTAL TACTICAL RESULT: " << passedTests << "/" << totalTests << " ("
              << (failedTests == 0 ? "ALL PASSED" : "FAILURES DETECTED") << ")\n";
    std::cout << "=================================================================\n";

    return (failedTests == 0);
}

} // namespace Boson

#endif // BOSON_TACTICAL_SUITE_CPP
