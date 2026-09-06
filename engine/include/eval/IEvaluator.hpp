#ifndef BOSON_I_EVALUATOR_HPP
#define BOSON_I_EVALUATOR_HPP

#include "board/Position.hpp"
#include "board/Move.hpp"

namespace Boson::eval {

class IEvaluator {
public:
    virtual ~IEvaluator() = default;
    [[nodiscard]] virtual int evaluate(const Position& pos) noexcept = 0;
    virtual void initializeSearch() noexcept {}
    virtual void initializeSearch(const Position& rootPos) noexcept { (void)rootPos; initializeSearch(); }
    virtual void notifyMove(const Position& before, const Position& after, const Move& move) noexcept {
        (void)before; (void)after; (void)move;
    }
    virtual void notifyUndo() noexcept {}
};

} // namespace Boson::eval

namespace boson::eval {
    using IEvaluator = Boson::eval::IEvaluator;
}

#endif // BOSON_I_EVALUATOR_HPP
