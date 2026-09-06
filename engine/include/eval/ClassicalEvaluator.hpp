#ifndef BOSON_CLASSICAL_EVALUATOR_HPP
#define BOSON_CLASSICAL_EVALUATOR_HPP

#include "eval/IEvaluator.hpp"

namespace Boson::eval {

class ClassicalEvaluator final : public IEvaluator {
public:
    ClassicalEvaluator() = default;
    ~ClassicalEvaluator() override = default;

    [[nodiscard]] int evaluate(const Position& pos) noexcept override;
    void initializeSearch() noexcept override;
};

} // namespace Boson::eval

namespace boson::eval {
    using ClassicalEvaluator = Boson::eval::ClassicalEvaluator;
}

#endif // BOSON_CLASSICAL_EVALUATOR_HPP
