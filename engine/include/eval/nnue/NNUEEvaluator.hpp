#ifndef BOSON_NNUE_EVALUATOR_HPP
#define BOSON_NNUE_EVALUATOR_HPP

#include "eval/IEvaluator.hpp"
#include "eval/nnue/NetworkModel.hpp"
#include "eval/nnue/AccumulatorStack.hpp"

#include <string>
#include <string_view>
#include <memory>

namespace Boson::eval::nnue {

[[nodiscard]] std::string computeSha256(const uint8_t* data, size_t length);
[[nodiscard]] std::string computeSha256(std::string_view data);
[[nodiscard]] std::string computeFileSha256(const std::string& path);
[[nodiscard]] std::string computeModelSha256(const NetworkModel& model);

[[nodiscard]] bool loadModelStrict(const std::string& path);
[[nodiscard]] bool loadModel(const std::string& path, bool strict = false);

class NNUEEvaluator final : public IEvaluator {
public:
    explicit NNUEEvaluator(const NetworkModel& model) noexcept;
    ~NNUEEvaluator() override = default;

    [[nodiscard]] int evaluate(const Position& pos) noexcept override;
    void initializeSearch() noexcept override;
    void initializeSearch(const Position& rootPos) noexcept override;
    void notifyMove(const Position& before, const Position& after, const Move& move) noexcept override;
    void notifyUndo() noexcept override;

    [[nodiscard]] const AccumulatorStack& getStack() const noexcept { return m_stack; }
    [[nodiscard]] AccumulatorStack& getMutableStack() noexcept { return m_stack; }
    [[nodiscard]] const NetworkModel& getModel() const noexcept { return m_model; }

    // Static Model Management & Identity
    static bool loadModelStrict(const std::string& path);
    static bool loadModel(const std::string& path, bool strict = false);
    [[nodiscard]] static const std::string& getActiveModelSha256() noexcept;
    [[nodiscard]] static const NetworkModel& getActiveModel() noexcept;
    [[nodiscard]] static bool hasLoadedModel() noexcept;
    static void setRequireNNUE(bool require) noexcept;
    [[nodiscard]] static bool isRequireNNUE() noexcept;
    static void setActiveModel(NetworkModel&& model, std::string sha256 = "");
    static void resetToSyntheticModel(uint32_t seed = 1337);
    static NNUEEvaluator& getInstance() noexcept;
    static void setHardExitOnFailure(bool enable) noexcept;
    [[nodiscard]] static bool isHardExitOnFailure() noexcept;

private:
    const NetworkModel& m_model;
    AccumulatorStack m_stack;
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using NNUEEvaluator = Boson::eval::nnue::NNUEEvaluator;
}

#endif // BOSON_NNUE_EVALUATOR_HPP
