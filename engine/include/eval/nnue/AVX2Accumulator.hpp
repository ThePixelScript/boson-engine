#ifndef BOSON_AVX2_ACCUMULATOR_HPP
#define BOSON_AVX2_ACCUMULATOR_HPP

#include "eval/nnue/Accumulator.hpp"
#include "board/Position.hpp"
#include "board/Color.hpp"
#include <vector>

namespace Boson::eval::nnue {

class AVX2Accumulator {
public:
    static void rebuildPerspective(AccumulatorHalf& outHalf,
                                   const Position& pos,
                                   Color perspective,
                                   const FeatureWeights& weights) noexcept;

    static void updateAccumulator(AccumulatorHalf& outHalf,
                                  const AccumulatorHalf& inHalf,
                                  const std::vector<int>& removed,
                                  const std::vector<int>& added,
                                  const FeatureWeights& weights) noexcept;

    [[nodiscard]] static bool verifyRangeSafe(const AccumulatorHalf& half) noexcept;
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using AVX2Accumulator = Boson::eval::nnue::AVX2Accumulator;
}

#endif // BOSON_AVX2_ACCUMULATOR_HPP
