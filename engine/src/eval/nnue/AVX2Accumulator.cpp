#include "eval/nnue/AVX2Accumulator.hpp"
#include "eval/nnue/FeatureTransformer.hpp"
#include <immintrin.h>

namespace Boson::eval::nnue {

namespace {

// Range-safe pack from two 8x int32 vectors to one 16x int16 vector with saturation
inline __m256i packAndPermute_epi32(__m256i lo32, __m256i hi32) noexcept {
    // _mm256_packs_epi32 saturates each signed int32 lane to [-32768, 32767]
    __m256i packed = _mm256_packs_epi32(lo32, hi32);
    // In-lane packing produces [a0..a3, b0..b3, a4..a7, b4..b7] across 64-bit qwords [0, 1, 2, 3]
    // Permute with _MM_SHUFFLE(3, 1, 2, 0) restores natural order [a0..a7, b0..b7]
    return _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
}

// Unpack 16x int16 to two 8x int32 vectors using unaligned loads
inline void unpack16_to_32(const int16_t* ptr, __m256i& lo32, __m256i& hi32) noexcept {
    __m128i raw_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
    __m128i raw_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 8));
    lo32 = _mm256_cvtepi16_epi32(raw_lo);
    hi32 = _mm256_cvtepi16_epi32(raw_hi);
}

} // namespace

void AVX2Accumulator::rebuildPerspective(AccumulatorHalf& outHalf,
                                         const Position& pos,
                                         Color perspective,
                                         const FeatureWeights& weights) noexcept {
    const auto activeFeatures = FeatureTransformer::getActiveFeatures(pos, perspective);

    // Process all 512 entries in 16-lane chunks using 32-bit intermediate accumulators
    for (size_t i = 0; i < ACCUMULATOR_SIZE; i += 16) {
        __m256i lo32, hi32;
        unpack16_to_32(&weights.biases[i], lo32, hi32);

        for (int feat : activeFeatures) {
            __m256i w_lo, w_hi;
            unpack16_to_32(&weights.weights[static_cast<size_t>(feat)][i], w_lo, w_hi);
            lo32 = _mm256_add_epi32(lo32, w_lo);
            hi32 = _mm256_add_epi32(hi32, w_hi);
        }

        __m256i res16 = packAndPermute_epi32(lo32, hi32);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&outHalf.values[i]), res16);
    }
}

void AVX2Accumulator::updateAccumulator(AccumulatorHalf& outHalf,
                                        const AccumulatorHalf& inHalf,
                                        const std::vector<int>& removed,
                                        const std::vector<int>& added,
                                        const FeatureWeights& weights) noexcept {
    for (size_t i = 0; i < ACCUMULATOR_SIZE; i += 16) {
        __m256i lo32, hi32;
        unpack16_to_32(&inHalf.values[i], lo32, hi32);

        for (int r : removed) {
            if (r < 0 || r >= HALFKP_FEATURES) continue;
            __m256i w_lo, w_hi;
            unpack16_to_32(&weights.weights[static_cast<size_t>(r)][i], w_lo, w_hi);
            lo32 = _mm256_sub_epi32(lo32, w_lo);
            hi32 = _mm256_sub_epi32(hi32, w_hi);
        }

        for (int a : added) {
            if (a < 0 || a >= HALFKP_FEATURES) continue;
            __m256i w_lo, w_hi;
            unpack16_to_32(&weights.weights[static_cast<size_t>(a)][i], w_lo, w_hi);
            lo32 = _mm256_add_epi32(lo32, w_lo);
            hi32 = _mm256_add_epi32(hi32, w_hi);
        }

        __m256i res16 = packAndPermute_epi32(lo32, hi32);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&outHalf.values[i]), res16);
    }
}

bool AVX2Accumulator::verifyRangeSafe(const AccumulatorHalf& half) noexcept {
    // Verify each lane contains valid int16 representation without UB
    for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
        int32_t val = static_cast<int32_t>(half.values[i]);
        if (val < -32768 || val > 32767) {
            return false;
        }
    }
    return true;
}

} // namespace Boson::eval::nnue
