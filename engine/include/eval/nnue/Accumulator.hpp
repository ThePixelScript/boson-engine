#ifndef BOSON_ACCUMULATOR_HPP
#define BOSON_ACCUMULATOR_HPP

#include "eval/nnue/NNUETypes.hpp"
#include "board/Color.hpp"
#include <array>
#include <cstdint>
#include <memory>

namespace Boson::eval::nnue {

struct alignas(64) AccumulatorHalf {
    std::array<int16_t, ACCUMULATOR_SIZE> values;
    void clear() noexcept { values.fill(0); }

    [[nodiscard]] bool operator==(const AccumulatorHalf& other) const noexcept {
        return values == other.values;
    }
    [[nodiscard]] bool operator!=(const AccumulatorHalf& other) const noexcept {
        return !(*this == other);
    }
};

struct alignas(64) Accumulator {
    AccumulatorHalf white;
    AccumulatorHalf black;

    [[nodiscard]] const AccumulatorHalf& get(Color c) const noexcept { return (c == Color::White) ? white : black; }
    [[nodiscard]] AccumulatorHalf& get(Color c) noexcept { return (c == Color::White) ? white : black; }

    [[nodiscard]] bool operator==(const Accumulator& other) const noexcept {
        return white == other.white && black == other.black;
    }
    [[nodiscard]] bool operator!=(const Accumulator& other) const noexcept {
        return !(*this == other);
    }
};

struct FeatureWeights {
    std::array<int16_t, ACCUMULATOR_SIZE> biases;
    std::array<std::array<int16_t, ACCUMULATOR_SIZE>, HALFKP_FEATURES> weights;

    [[nodiscard]] static std::unique_ptr<FeatureWeights> createDeterministic(uint64_t seed = 42);
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using AccumulatorHalf = Boson::eval::nnue::AccumulatorHalf;
    using Accumulator = Boson::eval::nnue::Accumulator;
    using FeatureWeights = Boson::eval::nnue::FeatureWeights;
}

#endif // BOSON_ACCUMULATOR_HPP
