#ifndef BOSON_FEATURE_TRANSFORMER_HPP
#define BOSON_FEATURE_TRANSFORMER_HPP

#include "eval/nnue/NNUETypes.hpp"
#include "board/Position.hpp"
#include "board/Move.hpp"
#include <vector>

namespace Boson::eval::nnue {

class FeatureTransformer {
public:
    [[nodiscard]] static std::vector<int> getActiveFeatures(const Position& pos, Color perspective);

    static void computeDeltas(const Position& before,
                              const Position& after,
                              const Move& move,
                              Color perspective,
                              std::vector<int>& removed,
                              std::vector<int>& added);
};

} // namespace Boson::eval::nnue

namespace boson::eval::nnue {
    using FeatureTransformer = Boson::eval::nnue::FeatureTransformer;
}

#endif // BOSON_FEATURE_TRANSFORMER_HPP
