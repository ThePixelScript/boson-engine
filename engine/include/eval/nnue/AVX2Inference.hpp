#ifndef BOSON_AVX2_INFERENCE_HPP
#define BOSON_AVX2_INFERENCE_HPP

#include "eval/nnue/NetworkModel.hpp"
#include "eval/nnue/Accumulator.hpp"
#include "eval/nnue/ScalarInference.hpp"
#include "board/Color.hpp"
#include <immintrin.h>
#include <cstdint>

namespace Boson::eval::nnue {

enum class InferenceBackend {
    Auto = 0,
    Scalar = 1,
    AVX2 = 2
};

class AVX2Inference {
public:
    [[nodiscard]] static bool isSupported() noexcept;

    static void setForceBackend(InferenceBackend backend) noexcept;
    [[nodiscard]] static InferenceBackend getActiveBackend() noexcept;

    [[nodiscard]] static int32_t evaluate(const Accumulator& acc,
                                          Color sideToMove,
                                          const NetworkModel& model) noexcept;

    [[nodiscard]] static LayerDiagnostics evaluateDetailed(const Accumulator& acc,
                                                           Color sideToMove,
                                                           const NetworkModel& model) noexcept;

    // Vectorized truncation division by 64 (truncates toward zero identically to C++ integer division)
    [[nodiscard]] static __m256i vecDiv64(__m256i x) noexcept {
        __m256i sign = _mm256_srai_epi32(x, 31);
        __m256i bias = _mm256_srli_epi32(sign, 26); // 63 if negative, 0 if non-negative
        __m256i sum = _mm256_add_epi32(x, bias);
        return _mm256_srai_epi32(sum, 6);
    }

    [[nodiscard]] static constexpr int32_t truncDiv64(int32_t x) noexcept {
        return x / 64;
    }

    [[nodiscard]] static constexpr int8_t crelu(int32_t x) noexcept {
        if (x < 0) return 0;
        if (x > 127) return 127;
        return static_cast<int8_t>(x);
    }
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using InferenceBackend = Boson::eval::nnue::InferenceBackend;
    using AVX2Inference = Boson::eval::nnue::AVX2Inference;
}

#endif // BOSON_AVX2_INFERENCE_HPP
