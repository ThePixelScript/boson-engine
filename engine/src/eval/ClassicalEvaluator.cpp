#include "eval/ClassicalEvaluator.hpp"
#include "evaluation/Evaluator.hpp"

namespace Boson::eval {

int ClassicalEvaluator::evaluate(const Position& pos) noexcept {
    return Evaluation::evaluate(pos);
}

void ClassicalEvaluator::initializeSearch() noexcept {
    // Stateless for classical evaluation
}

} // namespace Boson::eval
