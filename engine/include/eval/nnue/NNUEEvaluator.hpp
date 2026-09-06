#ifndef BOSON_NNUE_EVALUATOR_HPP
#define BOSON_NNUE_EVALUATOR_HPP

#include "eval/IEvaluator.hpp"
#include "eval/nnue/NetworkModel.hpp"
#include "eval/nnue/AccumulatorStack.hpp"

namespace Boson::eval::nnue {

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

private:
    const NetworkModel& m_model;
    AccumulatorStack m_stack;
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using NNUEEvaluator = Boson::eval::nnue::NNUEEvaluator;
}

#endif // BOSON_NNUE_EVALUATOR_HPP
