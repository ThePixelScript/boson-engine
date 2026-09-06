#include "eval/nnue/AVX2Inference.hpp"
#include <algorithm>
#include <atomic>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#include <x86intrin.h>
#endif

namespace Boson::eval::nnue {

namespace {

std::atomic<InferenceBackend> s_forcedBackend{InferenceBackend::Auto};

inline int32_t hsum8_epi32(__m256i v) noexcept {
    __m128i lo = _mm256_castsi256_si128(v);
    __m128i hi = _mm256_extracti128_si256(v, 1);
    __m128i sum128 = _mm_add_epi32(lo, hi);
    sum128 = _mm_hadd_epi32(sum128, sum128);
    sum128 = _mm_hadd_epi32(sum128, sum128);
    return _mm_cvtsi128_si32(sum128);
}

inline void activateHalf(const int16_t* inPtr, uint8_t* outPtr) noexcept {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i c127 = _mm256_set1_epi16(127);
    for (size_t i = 0; i < ACCUMULATOR_SIZE; i += 32) {
        __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(inPtr + i));
        __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(inPtr + i + 16));
        v0 = _mm256_max_epi16(v0, zero);
        v0 = _mm256_min_epi16(v0, c127);
        v1 = _mm256_max_epi16(v1, zero);
        v1 = _mm256_min_epi16(v1, c127);
        __m256i packed = _mm256_packus_epi16(v0, v1);
        __m256i ordered = _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(outPtr + i), ordered);
    }
}

} // namespace

bool AVX2Inference::isSupported() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    static const bool supported = []() -> bool {
#if defined(_MSC_VER)
        int cpuInfo[4] = {0};
        __cpuid(cpuInfo, 0);
        if (cpuInfo[0] < 7) return false;

        __cpuid(cpuInfo, 1);
        bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        bool avx = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx) return false;

        uint64_t xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6) return false;

        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
        return __builtin_cpu_supports("avx2");
#else
        return false;
#endif
    }();
    return supported;
#else
    return false;
#endif
}

void AVX2Inference::setForceBackend(InferenceBackend backend) noexcept {
    s_forcedBackend.store(backend, std::memory_order_relaxed);
}

InferenceBackend AVX2Inference::getActiveBackend() noexcept {
    return s_forcedBackend.load(std::memory_order_relaxed);
}

int32_t AVX2Inference::evaluate(const Accumulator& acc,
                                Color sideToMove,
                                const NetworkModel& model) noexcept {
    const AccumulatorHalf& us = acc.get(sideToMove);
    const AccumulatorHalf& them = acc.get(sideToMove == Color::White ? Color::Black : Color::White);

    alignas(64) std::array<uint8_t, FC1_INPUT_SIZE> input_activated;
    activateHalf(us.values.data(), &input_activated[0]);
    activateHalf(them.values.data(), &input_activated[ACCUMULATOR_SIZE]);

    // Layer 1: FC1 (1024 -> 32)
    alignas(32) std::array<int8_t, FC1_OUTPUT_SIZE> fc1_activated;
    const __m256i ones = _mm256_set1_epi16(1);

    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        __m256i acc32 = _mm256_setzero_si256();
        const auto& w = model.fc1_weights[j];
        for (size_t c = 0; c < FC1_INPUT_SIZE; c += 32) {
            __m256i in_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input_activated[c]));
            __m256i w_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&w[c]));
            __m256i prod16 = _mm256_maddubs_epi16(in_vec, w_vec);
            __m256i prod32 = _mm256_madd_epi16(prod16, ones);
            acc32 = _mm256_add_epi32(acc32, prod32);
        }
        int32_t raw = hsum8_epi32(acc32) + model.fc1_biases[j];
        fc1_activated[j] = crelu(truncDiv64(raw));
    }

    // Layer 2: FC2 (32 -> 32)
    alignas(32) std::array<int8_t, FC2_OUTPUT_SIZE> fc2_activated;
    __m256i in_fc1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(fc1_activated.data()));

    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        __m256i w_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(model.fc2_weights[k].data()));
        __m256i prod16 = _mm256_maddubs_epi16(in_fc1, w_vec);
        __m256i prod32 = _mm256_madd_epi16(prod16, ones);
        int32_t raw = hsum8_epi32(prod32) + model.fc2_biases[k];
        fc2_activated[k] = crelu(truncDiv64(raw));
    }

    // Layer 3: FC3 (32 -> 1)
    __m256i in_fc2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(fc2_activated.data()));
    __m256i w_fc3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(model.fc3_weights.data()));
    __m256i prod16 = _mm256_maddubs_epi16(in_fc2, w_fc3);
    __m256i prod32 = _mm256_madd_epi16(prod16, ones);
    int32_t fc3_raw = hsum8_epi32(prod32) + model.fc3_bias;

    int32_t score = fc3_raw * NNUE_OUTPUT_SCALE; // * 16
    return std::clamp<int32_t>(score, NNUE_EVAL_MIN, NNUE_EVAL_MAX);
}

LayerDiagnostics AVX2Inference::evaluateDetailed(const Accumulator& acc,
                                                 Color sideToMove,
                                                 const NetworkModel& model) noexcept {
    LayerDiagnostics diag{};

    const AccumulatorHalf& us = acc.get(sideToMove);
    const AccumulatorHalf& them = acc.get(sideToMove == Color::White ? Color::Black : Color::White);

    alignas(64) std::array<uint8_t, FC1_INPUT_SIZE> input_activated;
    activateHalf(us.values.data(), &input_activated[0]);
    activateHalf(them.values.data(), &input_activated[ACCUMULATOR_SIZE]);

    // Layer 1: FC1 (1024 -> 32)
    const __m256i ones = _mm256_set1_epi16(1);

    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        __m256i acc32 = _mm256_setzero_si256();
        const auto& w = model.fc1_weights[j];
        for (size_t c = 0; c < FC1_INPUT_SIZE; c += 32) {
            __m256i in_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&input_activated[c]));
            __m256i w_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&w[c]));
            __m256i prod16 = _mm256_maddubs_epi16(in_vec, w_vec);
            __m256i prod32 = _mm256_madd_epi16(prod16, ones);
            acc32 = _mm256_add_epi32(acc32, prod32);
        }
        int32_t raw = hsum8_epi32(acc32) + model.fc1_biases[j];
        diag.fc1_raw[j] = raw;
        diag.fc1_activated[j] = crelu(truncDiv64(raw));
    }

    // Layer 2: FC2 (32 -> 32)
    __m256i in_fc1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(diag.fc1_activated.data()));

    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        __m256i w_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(model.fc2_weights[k].data()));
        __m256i prod16 = _mm256_maddubs_epi16(in_fc1, w_vec);
        __m256i prod32 = _mm256_madd_epi16(prod16, ones);
        int32_t raw = hsum8_epi32(prod32) + model.fc2_biases[k];
        diag.fc2_raw[k] = raw;
        diag.fc2_activated[k] = crelu(truncDiv64(raw));
    }

    // Layer 3: FC3 (32 -> 1)
    __m256i in_fc2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(diag.fc2_activated.data()));
    __m256i w_fc3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(model.fc3_weights.data()));
    __m256i prod16 = _mm256_maddubs_epi16(in_fc2, w_fc3);
    __m256i prod32 = _mm256_madd_epi16(prod16, ones);
    int32_t fc3_raw = hsum8_epi32(prod32) + model.fc3_bias;
    diag.fc3_raw = fc3_raw;

    int32_t score = fc3_raw * NNUE_OUTPUT_SCALE; // * 16
    diag.final_score = std::clamp<int32_t>(score, NNUE_EVAL_MIN, NNUE_EVAL_MAX);

    return diag;
}

} // namespace Boson::eval::nnue
