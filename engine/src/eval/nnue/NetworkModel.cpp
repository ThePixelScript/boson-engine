#include "eval/nnue/NetworkModel.hpp"
#include <iostream>

namespace Boson::eval::nnue {

NetworkModel::NetworkModel() = default;
NetworkModel::~NetworkModel() = default;
NetworkModel::NetworkModel(NetworkModel&&) noexcept = default;
NetworkModel& NetworkModel::operator=(NetworkModel&&) noexcept = default;

NetworkModel createSyntheticModel(uint32_t seed) {
    NetworkModel model;
    model.featureWeights = FeatureWeights::createDeterministic(seed);

    uint64_t state = (seed == 0) ? 1337ULL : static_cast<uint64_t>(seed);
    auto nextRand = [&state]() -> uint64_t {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    };

    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        uint64_t r = nextRand();
        model.fc1_biases[j] = static_cast<int32_t>(-500 + static_cast<int32_t>(r % 1001));
        for (size_t i = 0; i < FC1_INPUT_SIZE; ++i) {
            uint64_t rw = nextRand();
            model.fc1_weights[j][i] = static_cast<int8_t>(-20 + static_cast<int32_t>(rw % 41));
        }
    }

    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        uint64_t r = nextRand();
        model.fc2_biases[k] = static_cast<int32_t>(-300 + static_cast<int32_t>(r % 601));
        for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
            uint64_t rw = nextRand();
            model.fc2_weights[k][j] = static_cast<int8_t>(-20 + static_cast<int32_t>(rw % 41));
        }
    }

    uint64_t rb = nextRand();
    model.fc3_bias = static_cast<int32_t>(-100 + static_cast<int32_t>(rb % 201));
    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        uint64_t rw = nextRand();
        model.fc3_weights[k] = static_cast<int8_t>(-20 + static_cast<int32_t>(rw % 41));
    }

    return model;
}

NetworkModel createSymmetricSyntheticModel(uint32_t seed) {
    NetworkModel model = createSyntheticModel(seed);

    // In a symmetric model:
    // Anti-symmetric weights between Us (first 512) and Them (next 512)
    // with 0 biases ensure that equal Us and Them inputs yield exactly zero score.
    for (size_t j = 0; j < FC1_OUTPUT_SIZE; ++j) {
        for (size_t i = 0; i < ACCUMULATOR_SIZE; ++i) {
            model.fc1_weights[j][ACCUMULATOR_SIZE + i] = static_cast<int8_t>(-model.fc1_weights[j][i]);
        }
        model.fc1_biases[j] = 0;
    }

    for (size_t k = 0; k < FC2_OUTPUT_SIZE; ++k) {
        model.fc2_biases[k] = 0;
    }
    model.fc3_bias = 0;

    return model;
}

void serializeModel(const NetworkModel& model, std::ostream& os) {
    const uint32_t header[7] = {
        NNUE_MAGIC,
        NNUE_VERSION,
        static_cast<uint32_t>(HALFKP_FEATURES),
        static_cast<uint32_t>(ACCUMULATOR_SIZE),
        static_cast<uint32_t>(FC1_INPUT_SIZE),
        static_cast<uint32_t>(FC1_OUTPUT_SIZE),
        static_cast<uint32_t>(FC2_OUTPUT_SIZE)
    };
    os.write(reinterpret_cast<const char*>(header), sizeof(header));

    if (model.featureWeights) {
        os.write(reinterpret_cast<const char*>(model.featureWeights->biases.data()),
                 sizeof(model.featureWeights->biases));
        os.write(reinterpret_cast<const char*>(model.featureWeights->weights.data()),
                 sizeof(model.featureWeights->weights));
    }

    os.write(reinterpret_cast<const char*>(model.fc1_biases.data()), sizeof(model.fc1_biases));
    os.write(reinterpret_cast<const char*>(model.fc1_weights.data()), sizeof(model.fc1_weights));
    os.write(reinterpret_cast<const char*>(model.fc2_biases.data()), sizeof(model.fc2_biases));
    os.write(reinterpret_cast<const char*>(model.fc2_weights.data()), sizeof(model.fc2_weights));
    os.write(reinterpret_cast<const char*>(&model.fc3_bias), sizeof(model.fc3_bias));
    os.write(reinterpret_cast<const char*>(model.fc3_weights.data()), sizeof(model.fc3_weights));
}

bool deserializeModel(NetworkModel& model, std::istream& is) {
    uint32_t header[7];
    is.read(reinterpret_cast<char*>(header), sizeof(header));
    if (is.gcount() != sizeof(header)) {
        return false;
    }

    // Fail-fast header validation BEFORE payload allocation
    if (header[0] != NNUE_MAGIC ||
        header[1] != NNUE_VERSION ||
        header[2] != static_cast<uint32_t>(HALFKP_FEATURES) ||
        header[3] != static_cast<uint32_t>(ACCUMULATOR_SIZE) ||
        header[4] != static_cast<uint32_t>(FC1_INPUT_SIZE) ||
        header[5] != static_cast<uint32_t>(FC1_OUTPUT_SIZE) ||
        header[6] != static_cast<uint32_t>(FC2_OUTPUT_SIZE)) {
        return false;
    }

    // Allocate payload only after header passes validation
    if (!model.featureWeights) {
        model.featureWeights = std::unique_ptr<FeatureWeights>(new FeatureWeights);
    }

    is.read(reinterpret_cast<char*>(model.featureWeights->biases.data()),
            sizeof(model.featureWeights->biases));
    if (is.gcount() != sizeof(model.featureWeights->biases)) return false;

    is.read(reinterpret_cast<char*>(model.featureWeights->weights.data()),
            sizeof(model.featureWeights->weights));
    if (is.gcount() != sizeof(model.featureWeights->weights)) return false;

    is.read(reinterpret_cast<char*>(model.fc1_biases.data()), sizeof(model.fc1_biases));
    if (is.gcount() != sizeof(model.fc1_biases)) return false;

    is.read(reinterpret_cast<char*>(model.fc1_weights.data()), sizeof(model.fc1_weights));
    if (is.gcount() != sizeof(model.fc1_weights)) return false;

    is.read(reinterpret_cast<char*>(model.fc2_biases.data()), sizeof(model.fc2_biases));
    if (is.gcount() != sizeof(model.fc2_biases)) return false;

    is.read(reinterpret_cast<char*>(model.fc2_weights.data()), sizeof(model.fc2_weights));
    if (is.gcount() != sizeof(model.fc2_weights)) return false;

    is.read(reinterpret_cast<char*>(&model.fc3_bias), sizeof(model.fc3_bias));
    if (is.gcount() != sizeof(model.fc3_bias)) return false;

    is.read(reinterpret_cast<char*>(model.fc3_weights.data()), sizeof(model.fc3_weights));
    if (is.gcount() != sizeof(model.fc3_weights)) return false;

    return true;
}

} // namespace Boson::eval::nnue
