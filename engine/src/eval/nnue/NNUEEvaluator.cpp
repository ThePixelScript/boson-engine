#include "eval/nnue/NNUEEvaluator.hpp"
#include "eval/nnue/ScalarInference.hpp"
#include "eval/nnue/AVX2Inference.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cstdlib>

namespace Boson::eval::nnue {

namespace {

// FIPS 180-4 SHA-256 round constants
static constexpr uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

[[nodiscard]] inline uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32 - n));
}

[[nodiscard]] inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

[[nodiscard]] inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

[[nodiscard]] inline uint32_t Sigma0(uint32_t x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

[[nodiscard]] inline uint32_t Sigma1(uint32_t x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

[[nodiscard]] inline uint32_t sigma0(uint32_t x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

[[nodiscard]] inline uint32_t sigma1(uint32_t x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

void transformBlock(uint32_t state[8], const uint8_t block[64]) noexcept {
    uint32_t W[64];
    for (int t = 0; t < 16; ++t) {
        W[t] = (static_cast<uint32_t>(block[t * 4]) << 24) |
               (static_cast<uint32_t>(block[t * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[t * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[t * 4 + 3]));
    }
    for (int t = 16; t < 64; ++t) {
        W[t] = sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t - 16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int t = 0; t < 64; ++t) {
        uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K256[t] + W[t];
        uint32_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static bool s_requireNNUE = false;
static bool s_hardExitOnFailure = true;
static bool s_hasLoadedModel = false;
static std::unique_ptr<NetworkModel> s_activeModel = nullptr;
static std::string s_activeModelSha256 = "";
static std::unique_ptr<NNUEEvaluator> s_activeEvaluator = nullptr;

} // anonymous namespace

std::string computeModelSha256(const NetworkModel& model) {
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    serializeModel(model, ss);
    std::string bytes = ss.str();
    return computeSha256(bytes);
}

std::string computeSha256(const uint8_t* data, size_t length) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    size_t fullBlocks = length / 64;
    for (size_t b = 0; b < fullBlocks; ++b) {
        transformBlock(state, data + b * 64);
    }

    uint8_t buffer[128] = {0};
    size_t rem = length % 64;
    if (rem > 0) {
        std::memcpy(buffer, data + fullBlocks * 64, rem);
    }
    buffer[rem] = 0x80;

    size_t padLen = (rem < 56) ? 64 : 128;
    uint64_t totalBits = static_cast<uint64_t>(length) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        buffer[padLen - 1 - i] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
    }

    transformBlock(state, buffer);
    if (padLen == 128) {
        transformBlock(state, buffer + 64);
    }

    std::ostringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << std::setw(8) << std::setfill('0') << state[i];
    }
    return ss.str();
}

std::string computeSha256(std::string_view data) {
    return computeSha256(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string computeFileSha256(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";
    std::streamsize size = file.tellg();
    if (size < 0) return "";
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return "";
    }
    return computeSha256(buffer.data(), buffer.size());
}

bool loadModel(const std::string& path, bool strict) {
    auto fail = [&](std::string_view reason) -> bool {
        if (strict || s_requireNNUE) {
            std::cerr << "[ERROR] NNUE strict load failed: " << reason << " ('" << path << "')\n";
            if (s_hardExitOnFailure && s_requireNNUE) {
                std::exit(1);
            }
        }
        return false;
    };

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return fail("File not found or inaccessible");
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize < 0) {
        return fail("Cannot determine file size");
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (fileSize > 0 && !file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return fail("Failed to read file payload");
    }

    std::string sha = computeSha256(buffer.data(), buffer.size());

    std::string strBuf(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    std::istringstream iss(strBuf, std::ios::binary);

    NetworkModel tempModel;
    if (!deserializeModel(tempModel, iss)) {
        return fail("Deserialization failed (invalid magic, version, dimensions, or corrupted bytes)");
    }

    NNUEEvaluator::setActiveModel(std::move(tempModel), std::move(sha));
    return true;
}

bool loadModelStrict(const std::string& path) {
    return loadModel(path, true);
}

NNUEEvaluator::NNUEEvaluator(const NetworkModel& model) noexcept
    : m_model(model), m_stack() {}

int NNUEEvaluator::evaluate(const Position& pos) noexcept {
    if (AVX2Inference::getActiveBackend() == InferenceBackend::AVX2 ||
        (AVX2Inference::getActiveBackend() == InferenceBackend::Auto && AVX2Inference::isSupported())) {
        return AVX2Inference::evaluate(m_stack.top(), pos.sideToMove(), m_model);
    }
    return ScalarInference::evaluate(m_stack.top(), pos.sideToMove(), m_model);
}

void NNUEEvaluator::initializeSearch() noexcept {
    // Stateless without root position
}

void NNUEEvaluator::initializeSearch(const Position& rootPos) noexcept {
    m_stack.reset(rootPos, *m_model.featureWeights);
}

void NNUEEvaluator::notifyMove(const Position& before, const Position& after, const Move& move) noexcept {
    m_stack.pushMove(before, after, move, *m_model.featureWeights);
}

void NNUEEvaluator::notifyUndo() noexcept {
    m_stack.pop();
}

bool NNUEEvaluator::loadModelStrict(const std::string& path) {
    return Boson::eval::nnue::loadModelStrict(path);
}

bool NNUEEvaluator::loadModel(const std::string& path, bool strict) {
    return Boson::eval::nnue::loadModel(path, strict);
}

const std::string& NNUEEvaluator::getActiveModelSha256() noexcept {
    if (s_activeModelSha256.empty()) {
        getInstance(); // Ensure initialized
    }
    return s_activeModelSha256;
}

const NetworkModel& NNUEEvaluator::getActiveModel() noexcept {
    if (!s_activeModel) {
        getInstance(); // Ensure initialized
    }
    return *s_activeModel;
}

bool NNUEEvaluator::hasLoadedModel() noexcept {
    return s_hasLoadedModel;
}

void NNUEEvaluator::setRequireNNUE(bool require) noexcept {
    s_requireNNUE = require;
}

bool NNUEEvaluator::isRequireNNUE() noexcept {
    return s_requireNNUE;
}

void NNUEEvaluator::setActiveModel(NetworkModel&& model, std::string sha256) {
    if (sha256.empty()) {
        sha256 = computeModelSha256(model);
    }
    s_activeModel = std::make_unique<NetworkModel>(std::move(model));
    s_activeModelSha256 = std::move(sha256);
    s_activeEvaluator = std::make_unique<NNUEEvaluator>(*s_activeModel);
    s_hasLoadedModel = true;
}

void NNUEEvaluator::resetToSyntheticModel(uint32_t seed) {
    auto model = createSyntheticModel(seed);
    std::string sha = computeModelSha256(model);
    s_activeModel = std::make_unique<NetworkModel>(std::move(model));
    s_activeModelSha256 = std::move(sha);
    s_activeEvaluator = std::make_unique<NNUEEvaluator>(*s_activeModel);
    s_hasLoadedModel = false;
}

NNUEEvaluator& NNUEEvaluator::getInstance() noexcept {
    if (!s_activeEvaluator) {
        resetToSyntheticModel(1337);
    }
    return *s_activeEvaluator;
}

void NNUEEvaluator::setHardExitOnFailure(bool enable) noexcept {
    s_hardExitOnFailure = enable;
}

bool NNUEEvaluator::isHardExitOnFailure() noexcept {
    return s_hardExitOnFailure;
}

} // namespace Boson::eval::nnue
