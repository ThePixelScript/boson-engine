#ifndef BOSON_ACCUMULATOR_STACK_HPP
#define BOSON_ACCUMULATOR_STACK_HPP

#include "eval/nnue/Accumulator.hpp"
#include "eval/nnue/FeatureTransformer.hpp"
#include "board/Position.hpp"
#include "board/Move.hpp"
#include <array>
#include <vector>
#include <cstddef>

namespace Boson::eval::nnue {

class AccumulatorStack {
public:
    static constexpr size_t MAX_STACK_DEPTH = 128;

    AccumulatorStack();

    void reset(const Position& rootPos, const FeatureWeights& weights) noexcept;
    void pushMove(const Position& before, const Position& after, const Move& move, const FeatureWeights& weights) noexcept;
    void pop() noexcept;

    [[nodiscard]] const Accumulator& top() const noexcept;
    [[nodiscard]] size_t currentPly() const noexcept;

    [[nodiscard]] const Accumulator& get(size_t ply) const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_t capacity() const noexcept;

    static void rebuildPerspective(AccumulatorHalf& outHalf,
                                   const Position& pos,
                                   Color perspective,
                                   const FeatureWeights& weights) noexcept;

private:
    std::array<Accumulator, MAX_STACK_DEPTH> m_stack;
    size_t m_currentPly{0};
    std::vector<int> m_removed;
    std::vector<int> m_added;
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using AccumulatorStack = Boson::eval::nnue::AccumulatorStack;
}

#endif // BOSON_ACCUMULATOR_STACK_HPP
