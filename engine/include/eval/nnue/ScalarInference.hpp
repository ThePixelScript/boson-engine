#ifndef BOSON_SCALAR_INFERENCE_HPP
#define BOSON_SCALAR_INFERENCE_HPP

#include "eval/nnue/NetworkModel.hpp"
#include "eval/nnue/Accumulator.hpp"
#include "board/Color.hpp"
#include <array>
#include <cstdint>

namespace Boson::eval::nnue {

struct LayerDiagnostics {
    std::array<int32_t, FC1_OUTPUT_SIZE> fc1_raw{};
    std::array<int8_t, FC1_OUTPUT_SIZE>  fc1_activated{};
    std::array<int32_t, FC2_OUTPUT_SIZE> fc2_raw{};
    std::array<int8_t, FC2_OUTPUT_SIZE>  fc2_activated{};
    int32_t fc3_raw{0};
    int32_t final_score{0};
};

class ScalarInference {
public:
    [[nodiscard]] static int32_t evaluate(const Accumulator& acc,
                                          Color sideToMove,
                                          const NetworkModel& model) noexcept;

    [[nodiscard]] static LayerDiagnostics evaluateDetailed(const Accumulator& acc,
                                                           Color sideToMove,
                                                           const NetworkModel& model) noexcept;

    [[nodiscard]] static constexpr int8_t crelu(int32_t x) noexcept {
        if (x < 0) return 0;
        if (x > 127) return 127;
        return static_cast<int8_t>(x);
    }
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using LayerDiagnostics = Boson::eval::nnue::LayerDiagnostics;
    using ScalarInference = Boson::eval::nnue::ScalarInference;
}

#endif // BOSON_SCALAR_INFERENCE_HPP
