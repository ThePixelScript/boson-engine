#include "eval/nnue/AccumulatorStack.hpp"
#include "eval/nnue/AVX2Accumulator.hpp"
#include "eval/nnue/AVX2Inference.hpp"
#include <cassert>

namespace Boson::eval::nnue {

namespace {
inline bool useAVX2() noexcept {
    return AVX2Inference::getActiveBackend() == InferenceBackend::AVX2 ||
           (AVX2Inference::getActiveBackend() == InferenceBackend::Auto && AVX2Inference::isSupported());
}
} // namespace

std::unique_ptr<FeatureWeights> FeatureWeights::createDeterministic(uint64_t seed) {
    auto fw = std::unique_ptr<FeatureWeights>(new FeatureWeights);

    uint64_t state = (seed == 0) ? 42ULL : seed;
    auto nextRand = [&state]() -> uint64_t {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    };

    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        uint64_t r = nextRand();
        fw->biases[i] = static_cast<int16_t>(-500 + static_cast<int32_t>(r % 1001));
    }

    for (size_t f = 0; f < HALFKP_FEATURES; ++f) {
        for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
            uint64_t r = nextRand();
            fw->weights[f][i] = static_cast<int16_t>(-200 + static_cast<int32_t>(r % 401));
        }
    }

    return fw;
}

AccumulatorStack::AccumulatorStack() {
    m_removed.reserve(64);
    m_added.reserve(64);
    for (auto& acc : m_stack) {
        acc.white.clear();
        acc.black.clear();
    }
}

void AccumulatorStack::rebuildPerspective(AccumulatorHalf& outHalf,
                                          const Position& pos,
                                          Color perspective,
                                          const FeatureWeights& weights) noexcept {
    if (useAVX2()) {
        AVX2Accumulator::rebuildPerspective(outHalf, pos, perspective, weights);
        return;
    }

    const auto activeFeatures = FeatureTransformer::getActiveFeatures(pos, perspective);

    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        outHalf.values[i] = weights.biases[i];
    }

    for (int feat : activeFeatures) {
        const auto& w = weights.weights[static_cast<size_t>(feat)];
        for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
            int32_t sum = static_cast<int32_t>(outHalf.values[i]) + static_cast<int32_t>(w[i]);
            outHalf.values[i] = static_cast<int16_t>(sum);
        }
    }
}

void AccumulatorStack::reset(const Position& rootPos, const FeatureWeights& weights) noexcept {
    m_currentPly = 0;
    rebuildPerspective(m_stack[0].white, rootPos, Color::White, weights);
    rebuildPerspective(m_stack[0].black, rootPos, Color::Black, weights);
}

void AccumulatorStack::pushMove(const Position& before,
                                const Position& after,
                                const Move& move,
                                const FeatureWeights& weights) noexcept {
    if (m_currentPly + 1 >= MAX_STACK_DEPTH) {
        return;
    }

    const size_t prevPly = m_currentPly;
    m_currentPly++;
    const Accumulator& prevAcc = m_stack[prevPly];
    Accumulator& currAcc = m_stack[m_currentPly];

    if (move.getRawData() == 0) {
        currAcc = prevAcc;
        return;
    }

    for (Color c : {Color::White, Color::Black}) {
        if (before.getKingSquare(c) != after.getKingSquare(c)) {
            // Per-Perspective King Rule:
            // For perspective c, RebuildPerspective(c) <=> posBefore.kingSquare(c) != posAfter.kingSquare(c)
            rebuildPerspective(currAcc.get(c), after, c, weights);
        } else {
            // Incremental delta update from previous accumulator
            FeatureTransformer::computeDeltas(before, after, move, c, m_removed, m_added);
            const AccumulatorHalf& prevHalf = prevAcc.get(c);
            AccumulatorHalf& currHalf = currAcc.get(c);

            if (useAVX2()) {
                AVX2Accumulator::updateAccumulator(currHalf, prevHalf, m_removed, m_added, weights);
            } else {
                currHalf = prevHalf;

                for (int r : m_removed) {
                    if (r < 0 || r >= HALFKP_FEATURES) continue;
                    const auto& w = weights.weights[static_cast<size_t>(r)];
                    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                        int32_t val = static_cast<int32_t>(currHalf.values[i]) - static_cast<int32_t>(w[i]);
                        currHalf.values[i] = static_cast<int16_t>(val);
                    }
                }

                for (int a : m_added) {
                    if (a < 0 || a >= HALFKP_FEATURES) continue;
                    const auto& w = weights.weights[static_cast<size_t>(a)];
                    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
                        int32_t val = static_cast<int32_t>(currHalf.values[i]) + static_cast<int32_t>(w[i]);
                        currHalf.values[i] = static_cast<int16_t>(val);
                    }
                }
            }
        }
    }
}

void AccumulatorStack::pop() noexcept {
    if (m_currentPly > 0) {
        m_currentPly--;
    }
}

const Accumulator& AccumulatorStack::top() const noexcept {
    assert(m_currentPly < MAX_STACK_DEPTH);
    return m_stack[m_currentPly];
}

size_t AccumulatorStack::currentPly() const noexcept {
    return m_currentPly;
}

const Accumulator& AccumulatorStack::get(size_t ply) const noexcept {
    assert(ply < MAX_STACK_DEPTH);
    return m_stack[ply];
}

bool AccumulatorStack::empty() const noexcept {
    return m_currentPly == 0;
}

size_t AccumulatorStack::capacity() const noexcept {
    return MAX_STACK_DEPTH;
}

} // namespace Boson::eval::nnue
