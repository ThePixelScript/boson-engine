#ifndef BOSON_SEARCH_STATISTICS_HPP
#define BOSON_SEARCH_STATISTICS_HPP

#include <cstdint>
#include <string>
#include "search/PVLine.hpp"

namespace Boson {

enum class StopReason {
    None,
    MaxDepthReached,
    DepthReached = MaxDepthReached,
    SoftTimeLimit,
    HardTimeLimit,
    ExternalStop,
    StopCommand = ExternalStop,
    NodesLimit
};

struct SearchStatistics {
    uint64_t nodes = 0;
    uint64_t qNodes = 0;
    uint64_t ttHits = 0;
    uint64_t betaCutoffs = 0;
    int64_t elapsedTimeMs = 0;
    int completedDepth = 0;
    std::string pvString = "";
    PVLine pvLine{};
    StopReason stopReason = StopReason::None;

    uint32_t aspirationSuccesses = 0;
    uint32_t failHighs = 0;
    uint32_t failLows = 0;
    uint32_t researchCount = 0;

    // Module 6.4 Charter Telemetry
    uint32_t aspirationResearches = 0;
    uint32_t aspirationFailHigh = 0;
    uint32_t aspirationFailLow = 0;

    uint64_t nullAttempts = 0;
    uint64_t nullCutoffs = 0;
    uint64_t nullFailures = 0;
    uint64_t nullDisabled = 0;

    // Module 6.6 Charter Telemetry
    uint64_t nullMoveAttempts = 0;
    uint64_t nullMoveCutoffs = 0;
    uint64_t nullMoveFailures = 0;

    // Module 6.7 Charter Telemetry: Late Move Reductions (LMR)
    uint64_t lmrAttempts = 0;
    uint64_t lmrResearches = 0;
    uint64_t successfulResearches = 0;
    uint64_t lmrReducedNodes = 0;

    // Backward-compatibility aliases
    uint64_t reducedNodes = 0;
    uint64_t researches = 0;

    uint64_t cmhHits = 0;
    uint64_t cmhCutoffs = 0;

    uint64_t conthistHits = 0;
    uint64_t conthistCutoffs = 0;
    uint32_t normalizationEvents = 0;

    // Module 6.10 Correction History Telemetry
    uint32_t corrUpdates{0};
    uint32_t corrApplied{0};
    uint32_t corrPositive{0};
    uint32_t corrNegative{0};
    int64_t corrTotalMagnitude{0};
    // Milestone Omega Telemetry Counters
    uint64_t staticEvalCalls{0};
    uint64_t seeInvocations{0};

    void reset() noexcept {
        nodes = qNodes = ttHits = betaCutoffs = elapsedTimeMs = completedDepth = 0;
        pvString = "";
        pvLine.count = 0;
        stopReason = StopReason::None;
        aspirationSuccesses = failHighs = failLows = researchCount = 0;
        aspirationResearches = aspirationFailHigh = aspirationFailLow = 0;
        nullAttempts = nullCutoffs = nullFailures = nullDisabled = 0;
        nullMoveAttempts = nullMoveCutoffs = nullMoveFailures = 0;
        lmrAttempts = lmrResearches = successfulResearches = lmrReducedNodes = reducedNodes = researches = 0;
        cmhHits = cmhCutoffs = 0;
        conthistHits = conthistCutoffs = normalizationEvents = 0;
        corrUpdates = corrApplied = corrPositive = corrNegative = 0;
        corrTotalMagnitude = 0;
        staticEvalCalls = seeInvocations = 0;
    }
};

} // namespace Boson
#endif // BOSON_SEARCH_STATISTICS_HPP