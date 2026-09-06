#ifndef BOSON_NETWORK_MODEL_HPP
#define BOSON_NETWORK_MODEL_HPP

#include "eval/nnue/NNUETypes.hpp"
#include "eval/nnue/Accumulator.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <iosfwd>

namespace Boson::eval::nnue {

constexpr uint32_t NNUE_MAGIC = 0x4E4E5545; // 'N','N','U','E'
constexpr uint32_t NNUE_VERSION = 1;
constexpr int32_t NNUE_OUTPUT_SCALE = 16;
constexpr int32_t NNUE_EVAL_MIN = -30000;
constexpr int32_t NNUE_EVAL_MAX =  30000;

constexpr int FC1_INPUT_SIZE  = 1024;
constexpr int FC1_OUTPUT_SIZE = 32;
constexpr int FC2_OUTPUT_SIZE = 32;

struct NetworkModel {
    std::unique_ptr<FeatureWeights> featureWeights;
    std::array<int32_t, FC1_OUTPUT_SIZE> fc1_biases{};
    std::array<std::array<int8_t, FC1_INPUT_SIZE>, FC1_OUTPUT_SIZE> fc1_weights{};
    std::array<int32_t, FC2_OUTPUT_SIZE> fc2_biases{};
    std::array<std::array<int8_t, FC2_OUTPUT_SIZE>, FC2_OUTPUT_SIZE> fc2_weights{};
    int32_t fc3_bias{0};
    std::array<int8_t, FC2_OUTPUT_SIZE> fc3_weights{};

    NetworkModel();
    ~NetworkModel();
    NetworkModel(NetworkModel&&) noexcept;
    NetworkModel& operator=(NetworkModel&&) noexcept;
    NetworkModel(const NetworkModel&) = delete;
    NetworkModel& operator=(const NetworkModel&) = delete;
};

[[nodiscard]] NetworkModel createSyntheticModel(uint32_t seed);
[[nodiscard]] NetworkModel createSymmetricSyntheticModel(uint32_t seed);
void serializeModel(const NetworkModel& model, std::ostream& os);
[[nodiscard]] bool deserializeModel(NetworkModel& model, std::istream& is);

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using namespace Boson::eval::nnue;
}

#endif // BOSON_NETWORK_MODEL_HPP
