#include "eval/nnue/ScalarInference.hpp"
#include <algorithm>

namespace Boson::eval::nnue {

LayerDiagnostics ScalarInference::evaluateDetailed(const Accumulator& acc,
                                                   Color sideToMove,
                                                   const NetworkModel& model) noexcept {
    LayerDiagnostics diag{};

    const AccumulatorHalf& us = acc.get(sideToMove);
    const AccumulatorHalf& them = acc.get(sideToMove == Color::White ? Color::Black : Color::White);

    // CReLU activation on input accumulator halves [Us | Them]
    alignas(64) std::array<int8_t, FC1_INPUT_SIZE> input_activated;
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        input_activated[i] = crelu(us.values[i]);
        input_activated[ACCUMULATOR_SIZE + i] = crelu(them.values[i]);
    }

    // Layer 1: FC1 (1024 -> 32)
    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        int32_t sum = model.fc1_biases[j];
        const auto& w = model.fc1_weights[j];
        for (size_t i = 0; i < FC1_INPUT_SIZE; ++i) {
            sum += static_cast<int32_t>(input_activated[i]) * static_cast<int32_t>(w[i]);
        }
        diag.fc1_raw[j] = sum;
        int32_t scaled = sum / 64; // truncates toward zero
        diag.fc1_activated[j] = crelu(scaled);
    }

    // Layer 2: FC2 (32 -> 32)
    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        int32_t sum = model.fc2_biases[k];
        const auto& w = model.fc2_weights[k];
        for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
            sum += static_cast<int32_t>(diag.fc1_activated[j]) * static_cast<int32_t>(w[j]);
        }
        diag.fc2_raw[k] = sum;
        int32_t scaled = sum / 64; // truncates toward zero
        diag.fc2_activated[k] = crelu(scaled);
    }

    // Layer 3: FC3 (32 -> 1)
    int32_t sum = model.fc3_bias;
    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        sum += static_cast<int32_t>(diag.fc2_activated[k]) * static_cast<int32_t>(model.fc3_weights[k]);
    }
    diag.fc3_raw = sum;

    // Output scaling and explicit bounds clamping
    int32_t score = sum * NNUE_OUTPUT_SCALE; // * 16
    diag.final_score = std::clamp<int32_t>(score, NNUE_EVAL_MIN, NNUE_EVAL_MAX);

    return diag;
}

int32_t ScalarInference::evaluate(const Accumulator& acc,
                                  Color sideToMove,
                                  const NetworkModel& model) noexcept {
    return evaluateDetailed(acc, sideToMove, model).final_score;
}

} // namespace Boson::eval::nnue
