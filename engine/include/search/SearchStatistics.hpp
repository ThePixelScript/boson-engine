#ifndef BOSON_SEARCH_STATISTICS_HPP
#define BOSON_SEARCH_STATISTICS_HPP

#include <cstdint>
#include <string>

namespace Boson {

enum class StopReason {
    None,
    MaxDepthReached,
    SoftTimeLimit,
    HardTimeLimit,
    ExternalStop
};

struct SearchStatistics {
    uint64_t nodes = 0;
    uint64_t qNodes = 0;
    uint64_t ttHits = 0;
    uint64_t betaCutoffs = 0;
    int64_t elapsedTimeMs = 0;
    int completedDepth = 0;
    std::string pvString = "";
    StopReason stopReason = StopReason::None;

    // Module 6.4: Aspiration Tuning Counters
    uint32_t aspirationSuccesses = 0;
    uint32_t failHighs = 0;
    uint32_t failLows = 0;
    uint32_t researchCount = 0;

    // Module 6.6: Null Move Pruning Telemetry
    uint64_t nullAttempts = 0;
    uint64_t nullCutoffs = 0;
    uint64_t nullFailures = 0;
    uint64_t nullDisabled = 0;

    void reset() noexcept {
        nodes = 0;
        qNodes = 0;
        ttHits = 0;
        betaCutoffs = 0;
        elapsedTimeMs = 0;
        completedDepth = 0;
        pvString = "";
        stopReason = StopReason::None;
        aspirationSuccesses = 0;
        failHighs = 0;
        failLows = 0;
        researchCount = 0;
        nullAttempts = 0;
        nullCutoffs = 0;
        nullFailures = 0;
        nullDisabled = 0;
    }
};

} // namespace Boson

#endif // BOSON_SEARCH_STATISTICS_HPP