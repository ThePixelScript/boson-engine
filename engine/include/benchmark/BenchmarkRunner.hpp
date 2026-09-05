#ifndef BOSON_BENCHMARK_RUNNER_HPP
#define BOSON_BENCHMARK_RUNNER_HPP

#include "benchmark/BenchmarkTypes.hpp"
#include "benchmark/BenchmarkCorpus.hpp"
#include <string>

namespace Boson {

struct BenchmarkConfig {
    int overrideDepth{0};
    size_t hashSizeMb{16};
    BenchmarkStateMode mode{BenchmarkStateMode::Isolated};
    int threads{1};
    std::string jsonOutputFile{};
    bool printConsole{true};
    bool silentSearch{true};
};

class BenchmarkRunner {
public:
    static BenchmarkRunRecord run(const BenchmarkConfig& config = BenchmarkConfig{});
    static void resetSearchState(size_t hashSizeMb = 16) noexcept;
};

} // namespace Boson

#endif // BOSON_BENCHMARK_RUNNER_HPP
