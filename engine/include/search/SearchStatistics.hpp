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

    void reset() noexcept {
        nodes = 0;
        qNodes = 0;
        ttHits = 0;
        betaCutoffs = 0;
        elapsedTimeMs = 0;
        completedDepth = 0;
        pvString = "";
        stopReason = StopReason::None;
    }
};

} // namespace Boson

#endif // BOSON_SEARCH_STATISTICS_HPP