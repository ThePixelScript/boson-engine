#ifndef BOSON_SEARCH_STATISTICS_HPP
#define BOSON_SEARCH_STATISTICS_HPP

#include <cstdint>
#include <string>

namespace Boson {

enum class StopReason { None, MaxDepthReached, SoftTimeLimit, HardTimeLimit, ExternalStop };

struct SearchStatistics {
    uint64_t nodes = 0;
    uint64_t qNodes = 0;
    uint64_t ttHits = 0;
    uint64_t betaCutoffs = 0;
    int64_t elapsedTimeMs = 0;
    int completedDepth = 0;
    std::string pvString = "";
    StopReason stopReason = StopReason::None;

    uint32_t aspirationSuccesses = 0;
    uint32_t failHighs = 0;
    uint32_t failLows = 0;
    uint32_t researchCount = 0;

    uint64_t nullAttempts = 0;
    uint64_t nullCutoffs = 0;
    uint64_t nullFailures = 0;
    uint64_t nullDisabled = 0;

    uint64_t lmrAttempts = 0;
    uint64_t reducedNodes = 0;
    uint64_t researches = 0;
    uint64_t successfulResearches = 0;

    // Module 6.8 Telemetry
    uint64_t cmhHits = 0;
    uint64_t cmhCutoffs = 0;

    void reset() noexcept {
        nodes = qNodes = ttHits = betaCutoffs = elapsedTimeMs = completedDepth = 0;
        pvString = "";
        stopReason = StopReason::None;
        aspirationSuccesses = failHighs = failLows = researchCount = 0;
        nullAttempts = nullCutoffs = nullFailures = nullDisabled = 0;
        lmrAttempts = reducedNodes = researches = successfulResearches = 0;
        cmhHits = cmhCutoffs = 0;
    }
};

} // namespace Boson
#endif // BOSON_SEARCH_STATISTICS_HPP