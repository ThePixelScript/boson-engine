#ifndef BOSON_VERIFICATION_HARNESS_HPP
#define BOSON_VERIFICATION_HARNESS_HPP

#include "board/Position.hpp"
#include <string>
#include <vector>

namespace Boson {

struct PerftTestCase {
    std::string fen;
    int depth;
    uint64_t expectedNodes;
    std::string label;
};

class VerificationHarness {
public:
    static bool runComprehensivePerft() noexcept;
    static void executePerformanceBenchmark() noexcept;

private:
    static const std::vector<PerftTestCase> S_PERFT_SUITE;
};

} // namespace Boson

#endif // BOSON_VERIFICATION_HARNESS_HPP