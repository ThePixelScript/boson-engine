#include "eval/ClassicalEvaluator.hpp"
#include "evaluation/Evaluator.hpp"

namespace Boson::eval {

int ClassicalEvaluator::evaluate(const Position& pos) noexcept {
    return Evaluation::evaluate(pos);
}

void ClassicalEvaluator::initializeSearch() noexcept {
    // Stateless for classical evaluation
}

void ClassicalEvaluator::initializeSearch(const Position& rootPos) noexcept {
    (void)rootPos;
    // Stateless for classical evaluation
}

void ClassicalEvaluator::notifyMove(const Position& before, const Position& after, const Move& move) noexcept {
    (void)before;
    (void)after;
    (void)move;
    // Stateless for classical evaluation
}

void ClassicalEvaluator::notifyUndo() noexcept {
    // Stateless for classical evaluation
}

} // namespace Boson::eval
