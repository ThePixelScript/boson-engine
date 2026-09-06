#include "eval/nnue/NNUEEvaluator.hpp"
#include "eval/nnue/ScalarInference.hpp"

namespace Boson::eval::nnue {

NNUEEvaluator::NNUEEvaluator(const NetworkModel& model) noexcept
    : m_model(model), m_stack() {}

int NNUEEvaluator::evaluate(const Position& pos) noexcept {
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

} // namespace Boson::eval::nnue
