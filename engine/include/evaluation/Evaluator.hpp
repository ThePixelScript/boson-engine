#ifndef BOSON_EVALUATOR_HPP
#define BOSON_EVALUATOR_HPP

#include "board/Position.hpp"

namespace Boson {

class Evaluator {
public:
    // Pure, stateless evaluation interface returning centipawn score from White's perspective
    static int evaluate(const Position& pos) noexcept;

    // Configurable material weights
    static constexpr int PAWN_VALUE   = 100;
    static constexpr int KNIGHT_VALUE = 320;
    static constexpr int BISHOP_VALUE = 330;
    static constexpr int ROOK_VALUE   = 500;
    static constexpr int QUEEN_VALUE  = 900;
};

} // namespace Boson

#endif // BOSON_EVALUATOR_HPP