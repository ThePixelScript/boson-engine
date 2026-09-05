#ifndef BOSON_BENCHMARK_TYPES_HPP
#define BOSON_BENCHMARK_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Boson {

enum class BenchmarkStateMode : uint8_t {
    Isolated,   ///< Default canonical mode: full search heuristic reset prior to each position
    Persistent  ///< Exploratory mode: TT and heuristic state persist across sequential positions
};

struct DeterministicTelemetry {
    uint64_t totalNodes{0};
    uint64_t totalQNodes{0};
    int completedDepth{0};
    int scoreCp{0};
    std::string bestMoveUci{};
    std::string pvString{};
    uint64_t ttHits{0};
    uint64_t ttCutoffs{0};
    uint64_t betaCutoffs{0};
    uint64_t nmpAttempts{0};
    uint64_t nmpCutoffs{0};
    uint64_t lmrReductions{0};
    uint64_t lmrResearches{0};
    uint64_t seeCalls{0};
    uint64_t seeRejections{0};

    bool operator==(const DeterministicTelemetry& rhs) const noexcept {
        return totalNodes == rhs.totalNodes
            && totalQNodes == rhs.totalQNodes
            && completedDepth == rhs.completedDepth
            && scoreCp == rhs.scoreCp
            && bestMoveUci == rhs.bestMoveUci
            && ttHits == rhs.ttHits
            && ttCutoffs == rhs.ttCutoffs
            && betaCutoffs == rhs.betaCutoffs
            && nmpAttempts == rhs.nmpAttempts
            && nmpCutoffs == rhs.nmpCutoffs
            && lmrReductions == rhs.lmrReductions
            && lmrResearches == rhs.lmrResearches
            && seeCalls == rhs.seeCalls
            && seeRejections == rhs.seeRejections;
    }
};

struct EnvironmentalTelemetry {
    int64_t elapsedMilliseconds{0};
    uint64_t nodesPerSecond{0};
    size_t memoryAllocatedMb{16};
};

struct PositionBenchmarkResult {
    std::string id{};
    std::string fen{};
    int targetDepth{0};
    DeterministicTelemetry deterministic{};
    EnvironmentalTelemetry environmental{};
};

struct AggregateBenchmarkResult {
    uint64_t totalNodes{0};
    uint64_t totalQNodes{0};
    int64_t elapsedMilliseconds{0};
    uint64_t nodesPerSecond{0};
    uint64_t ttHits{0};
    uint64_t betaCutoffs{0};
    uint64_t nmpAttempts{0};
    uint64_t nmpCutoffs{0};
    uint64_t lmrReductions{0};
    uint64_t lmrResearches{0};
    uint64_t seeCalls{0};
    size_t positionCount{0};
};

struct BenchmarkRunRecord {
    std::string schemaVersion{"1.0.0"};
    std::string corpusVersion{"1.0.0"};
    std::string engineName{"Boson"};
    std::string engineVersion{"0.8.0-dev"};
    std::string compiler{};
    std::string architecture{};
    std::string instructionSets{};
    BenchmarkStateMode mode{BenchmarkStateMode::Isolated};
    size_t hashSizeMb{16};
    int threads{1};
    std::string serializedParameters{};
    std::vector<PositionBenchmarkResult> positions{};
    AggregateBenchmarkResult aggregate{};
};

} // namespace Boson

#endif // BOSON_BENCHMARK_TYPES_HPP
