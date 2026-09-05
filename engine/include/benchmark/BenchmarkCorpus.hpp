#ifndef BOSON_BENCHMARK_CORPUS_HPP
#define BOSON_BENCHMARK_CORPUS_HPP

#include <string_view>
#include <span>
#include <cstdint>

namespace Boson {

enum class PositionCategory : uint8_t {
    Standard,
    Tactical,
    Positional,
    Endgame,
    SearchStress
};

struct BenchmarkPosition {
    std::string_view id{};
    std::string_view fen{};
    PositionCategory category{PositionCategory::Standard};
    int defaultDepth{10};
};

class BenchmarkCorpus {
public:
    static constexpr std::string_view VERSION = "1.0.0";

    [[nodiscard]] static std::string_view getVersion() noexcept {
        return VERSION;
    }

    [[nodiscard]] static std::span<const BenchmarkPosition> getPositions() noexcept;
    [[nodiscard]] static std::string_view getCategoryName(PositionCategory category) noexcept;
};

} // namespace Boson

#endif // BOSON_BENCHMARK_CORPUS_HPP
