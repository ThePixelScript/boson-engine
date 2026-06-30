#ifndef BOSON_VERIFICATION_HARNESS_HPP
#define BOSON_VERIFICATION_HARNESS_HPP

#include "board/Position.hpp"
#include "board/Move.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace Boson {

bool verifyBoardIntegrity(const Position& pos, const BoardSnapshot& expected) noexcept;
bool verifyMakeUndoCycle(Position& pos, const Move& move) noexcept;

struct PerftTestCase {
    std::string fenString;
    int targetDepth;
    uint64_t nodesExpected;
    std::string testLabel;
};

class VerificationHarness {
public:
    static bool runComprehensivePerft() noexcept;
    static bool verifyMakeUndoIntegrity(const std::string& fen, int depth) noexcept;
    static void debugPerft(const std::string& fen, int depth);
    static void executePerformanceBenchmark() noexcept;
};

} // namespace Boson

#endif