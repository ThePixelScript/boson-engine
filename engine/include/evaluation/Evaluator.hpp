#ifndef BOSON_EVALUATOR_HPP
#define BOSON_EVALUATOR_HPP

#include "board/Position.hpp"
#include "evaluation/CorrectionHistoryTable.hpp"

namespace Boson {

class Evaluator {
public:
    static constexpr int PAWN_VALUE = 100;
    static constexpr int KNIGHT_VALUE = 300;
    static constexpr int BISHOP_VALUE = 325;
    static constexpr int ROOK_VALUE = 500;
    static constexpr int QUEEN_VALUE = 900;

    [[nodiscard]] static int evaluate(const Position& pos) noexcept;
    [[nodiscard]] static int evaluateWithCorrection(const Position& pos) noexcept { return evaluate(pos); }
    
    [[nodiscard]] static CorrectionHistoryTable& getCorrHist() noexcept { return s_corrTable; }
    [[nodiscard]] static const CorrectionHistoryTable& getConstCorrHist() noexcept { return s_corrTable; }
    [[nodiscard]] static CorrectionHistoryTable& getMutableCorrHist() noexcept { return s_corrTable; }

private:
    static inline CorrectionHistoryTable s_corrTable{};
};

using Evaluation = Evaluator;

} // namespace Boson

namespace boson {
    using Evaluator = Boson::Evaluator;
    using Evaluation = Boson::Evaluator;
}

#endif // BOSON_EVALUATOR_HPP