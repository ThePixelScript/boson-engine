#ifndef BOSON_I_EVALUATOR_HPP
#define BOSON_I_EVALUATOR_HPP

#include "board/Position.hpp"

namespace Boson::eval {

class IEvaluator {
public:
    virtual ~IEvaluator() = default;
    [[nodiscard]] virtual int evaluate(const Position& pos) noexcept = 0;
    virtual void initializeSearch() noexcept {}
};

} // namespace Boson::eval

namespace boson::eval {
    using IEvaluator = Boson::eval::IEvaluator;
}

#endif // BOSON_I_EVALUATOR_HPP
