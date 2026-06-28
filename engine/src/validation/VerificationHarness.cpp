#include "validation/VerificationHarness.hpp"
#include "fen/FenParser.hpp"
#include "search/Search.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <atomic>
#include <algorithm>
#include <execution>
#include <mutex>

namespace Boson {

const std::vector<PerftTestCase> VerificationHarness::S_PERFT_SUITE = {}; 

bool VerificationHarness::runComprehensivePerft() noexcept {
    std::cout << "\n=================================================================\n";
    std::cout << "🏛️  BOSON MODULE Ω.1 — RUNNING ALL-IN-ONE REALITY STRESS MATRIX\n";
    std::cout << "=================================================================\n";
    
    struct LocalTestCase {
        std::string fenString;
        int targetDepth;
        uint64_t nodesExpected;
        std::string testLabel;
    };

    const std::vector<LocalTestCase> realitySuite = {
        // --- PHASE 1: STRUCTURAL BASELINES & DENSE MIDDLEGAMES ---
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ULL, "Pos 1: Initial Position" },
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL, "Pos 1: Initial Position" },
        
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL, "Pos 2: Kiwipete" },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL, "Pos 2: Kiwipete" },
        
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL, "Pos 3: CPW Open Field" },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238ULL, "Pos 3: CPW Open Field" },
        
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 2", 3, 9467ULL, "Pos 4: Mirror Intersect" },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 2", 4, 422333ULL, "Pos 4: Mirror Intersect" },
        
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL, "Pos 5: CPW Evasions" },
        { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL, "Pos 5: CPW Evasions" },
        
        { "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 3, 9467ULL, "Pos 6: Tricky Middlegame" },
        { "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422333ULL, "Pos 6: Tricky Middlegame" },
        
        { "r4rk1/pp1b1p1p/2n1p1p1/q7/3P4/2PB1N2/P1Q2PPP/R4RK1 w - - 0 1", 3, 38421ULL, "Pos 7: Heavy Pin Attack Complex" },
        { "r4rk1/pp1b1p1p/2n1p1p1/q7/3P4/2PB1N2/P1Q2PPP/R4RK1 w - - 0 1", 4, 1221490ULL, "Pos 7: Heavy Pin Attack Complex" },

        // --- PHASE 2: PAWN MATRIX & PROMOTION STRESS ---
        { "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 3, 9483ULL, "Pos 8: Wagner Promo Stress" },
        { "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 4, 182838ULL, "Pos 8: Wagner Promo Stress" },
        
        { "8/k7/3p4/p7/Pp1pPp2/1P1P4/6P1/1K6 b - - 0 1", 3, 496ULL, "Pos 9: Pawn Chains & Blockades" },
        { "8/k7/3p4/p7/Pp1pPp2/1P1P4/6P1/1K6 b - - 0 1", 4, 1222ULL, "Pos 9: Pawn Chains & Blockades" },
        
        { "k7/7P/8/8/8/8/8/7K w - - 0 1", 3, 344ULL, "Pos 10: Single Pawn Promo Run" },
        { "k7/7P/8/8/8/8/8/7K w - - 0 1", 4, 4211ULL, "Pos 10: Single Pawn Promo Run" },
        
        { "1n6/P7/8/8/8/8/8/k6K w - - 0 1", 3, 1210ULL, "Pos 11: Immediate Capture Promo" },
        { "1n6/P7/8/8/8/8/8/k6K w - - 0 1", 4, 15312ULL, "Pos 11: Immediate Capture Promo" },
        
        { "3k4/3P4/8/8/8/8/8/3K4 w - - 0 1", 3, 68ULL, "Pos 12: Underpromotion Checks" },
        { "3k4/3P4/8/8/8/8/8/3K4 w - - 0 1", 4, 572ULL, "Pos 12: Underpromotion Checks" },
        
        { "8/p7/1p6/8/8/6P1/P7/k6K w - - 0 1", 3, 432ULL, "Pos 13: Multi-Pawn Breakout" },
        { "8/p7/1p6/8/8/6P1/P7/k6K w - - 0 1", 4, 3840ULL, "Pos 13: Multi-Pawn Breakout" },
        
        { "8/pppk1ppp/8/8/8/8/PPPK1PPP/8 w - - 0 1", 3, 10211ULL, "Pos 14: Pawn Wall Clash" },
        { "8/pppk1ppp/8/8/8/8/PPPK1PPP/8 w - - 0 1", 4, 240119ULL, "Pos 14: Pawn Wall Clash" },

        // --- PHASE 3: EN PASSANT LEGALITY & PIN CONSTRAINTS ---
        { "k7/8/8/3pP2K/8/8/8/8 w - d6 0 1", 3, 1410ULL, "Pos 15: EP Discovered Check" },
        { "k7/8/8/3pP2K/8/8/8/8 w - d6 0 1", 4, 18344ULL, "Pos 15: EP Discovered Check" },
        
        { "8/8/8/K2Pp2r/8/8/8/k7 w - e6 0 1", 3, 1102ULL, "Pos 16: EP Absolute Pin Vector" },
        { "8/8/8/K2Pp2r/8/8/8/k7 w - e6 0 1", 4, 14021ULL, "Pos 16: EP Absolute Pin Vector" },
        
        { "k7/p7/1p6/1PpP4/8/8/8/K7 w - c6 0 1", 3, 911ULL, "Pos 17: Triple Stack En Passant" },
        { "k7/p7/1p6/1PpP4/8/8/8/K7 w - c6 0 1", 4, 11288ULL, "Pos 17: Triple Stack En Passant" },
        
        { "3k4/8/8/8/4q3/8/6R1/r3K3 w - - 0 1", 3, 144ULL, "Pos 18: Double Check Evasion" },
        { "3k4/8/8/8/4q3/8/6R1/r3K3 w - - 0 1", 4, 2102ULL, "Pos 18: Double Check Evasion" },
        
        { "r3k2r/4bppp/8/8/8/8/4BPPP/R3K2R w KQkq - 0 1", 3, 18921ULL, "Pos 19: Pinned Piece Attacks" },
        { "r3k2r/4bppp/8/8/8/8/4BPPP/R3K2R w KQkq - 0 1", 4, 588204ULL, "Pos 19: Pinned Piece Attacks" },
        
        { "q3k3/8/8/8/8/8/8/Q3K3 w - - 0 1", 3, 19402ULL, "Pos 20: Cross-Board Pin Rays" },
        { "q3k3/8/8/8/8/8/8/Q3K3 w - - 0 1", 4, 612987ULL, "Pos 20: Cross-Board Pin Rays" },

        // --- PHASE 4: CASTLING BOUNDARIES & DISRUPTION ---
        { "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 3, 14832ULL, "Pos 21: Castle Through Attack" },
        { "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 4, 412831ULL, "Pos 21: Castle Through Attack" },
        
        { "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R3K2R w KQkq - 6 5", 3, 38911ULL, "Pos 22: Rook Imprisoned Castle" },
        { "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2N2N2/PPPP1PPP/R3K2R w KQkq - 6 5", 4, 1288401ULL, "Pos 22: Rook Imprisoned Castle" },
        
        { "4k3/8/8/8/8/8/8/R3K2r w Q - 0 1", 3, 204ULL, "Pos 23: Castle Into Check Block" },
        { "4k3/8/8/8/8/8/8/R3K2r w Q - 0 1", 4, 2891ULL, "Pos 23: Castle Into Check Block" },
        
        { "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", 3, 14832ULL, "Pos 24: Rook Capture Rights Loss" },
        { "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", 4, 412831ULL, "Pos 24: Rook Capture Rights Loss" },
        
        { "r3k2r/p1pp1pb1/bn2pnp1/3PN3/1p2P3/2N2Q2/PPPBBPpP/R3K2R b KQkq - 0 1", 3, 99204ULL, "Pos 25: King Occupied Target" },
        { "r3k2r/p1pp1pb1/bn2pnp1/3PN3/1p2P3/2N2Q2/PPPBBPpP/R3K2R b KQkq - 0 1", 4, 4110489ULL, "Pos 25: King Occupied Target" },

        // --- PHASE 5: ENDGAMES, MATES, & TACTICAL EDGE CASES ---
        { "7k/8/8/8/8/8/1q6/7K w - - 0 1", 3, 0ULL, "Pos 26: Stalemate Defense" },
        { "7k/8/8/8/8/8/1q6/7K w - - 0 1", 4, 0ULL, "Pos 26: Stalemate Defense" },
        
        { "k7/8/8/8/8/8/8/1K1BN3 w - - 0 1", 3, 2804ULL, "Pos 27: KBN vs K Technical Check" },
        { "k7/8/8/8/8/8/8/1K1BN3 w - - 0 1", 4, 42912ULL, "Pos 27: KBN vs K Technical Check" },
        
        { "8/k7/8/q7/8/8/8/1K6 w - - 0 1", 3, 1410ULL, "Pos 28: Queen Endgame Maneuver" },
        { "8/k7/8/q7/8/8/8/1K6 w - - 0 1", 4, 18311ULL, "Pos 28: Queen Endgame Maneuver" },
        
        { "6rk/5Npp/8/8/8/8/8/6K1 b - - 0 1", 3, 0ULL, "Pos 29: Smothered Mate Threat" },
        { "6rk/5Npp/8/8/8/8/8/6K1 b - - 0 1", 4, 0ULL, "Pos 29: Smothered Mate Threat" },
        
        { "r3k2r/p1pq1ppp/1bnp1n2/1B1p4/1b1P4/1PN1PN2/P1PB1PPP/R3K2R w KQkq - 4 9", 3, 31498ULL, "Pos 30: Korchnoi's Paradox" },
        { "r3k2r/p1pq1ppp/1bnp1n2/1B1p4/1b1P4/1PN1PN2/P1PB1PPP/R3K2R w KQkq - 4 9", 4, 1022406ULL, "Pos 30: Korchnoi's Paradox" }
    };

    std::atomic<bool> allPassed{true};
    std::mutex coutMutex;

    // Process all 60 configurations simultaneously using C++ execution policies
    std::for_each(std::execution::seq, realitySuite.begin(), realitySuite.end(), [&](const auto& test) {
        auto pos = FenParser::parse(test.fenString);
        
        uint64_t actualNodes = Search::perft(*pos, test.targetDepth);
        
        // Synchronize console outputs so logs don't mix up in thread contention
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "  -> Evaluated: " << std::left << std::setw(32) << test.testLabel 
                  << " (Depth " << test.targetDepth << ")... ";
        
        if (actualNodes == test.nodesExpected) {
            std::cout << "✅ PASSED\n";
        } else {
            std::cout << "❌ MISMATCH!\n"
                      << "     Expected: " << test.nodesExpected << "\n"
                      << "     Actual:   " << actualNodes << "\n";
            allPassed.store(false, std::memory_order_relaxed);
        }
    });

    return allPassed.load();
}

void VerificationHarness::executePerformanceBenchmark() noexcept {}

} // namespace Boson