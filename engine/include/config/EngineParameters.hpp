#ifndef BOSON_ENGINE_PARAMETERS_HPP
#define BOSON_ENGINE_PARAMETERS_HPP

#include <cstdint>

namespace Boson {

struct SearchParameters {
    double lmrBase = 0.5;
    double lmrDivisor = 1.95;
    int lmrMinDepth = 3;
    int lmrMinMoveCount = 4;
    int nmpMinDepth = 3;
    int nmpReduction = 2;
    int aspirationInitialDelta = 30;
    int aspirationMaxDelta = 400;
    int killerSlotCount = 2;
    int rfpMarginBase = 75;
};

struct EvaluationParameters {
    int pawnValue = 100;
    int knightValue = 320;
    int bishopValue = 330;
    int rookValue = 500;
    int queenValue = 900;
    int maxCorrection = 1024;
    int corrScaleFactor = 256;
};

struct TimeParameters {
    uint64_t nodeCheckPeriod = 2048;
    int allocDivisor = 20;
    int incDivisor = 2;
    double hardLimitMultiplier = 3.0;
};

struct DebugParameters {
    bool enableNMP = true;
    bool enableLMR = true;
    bool enableAspiration = true;
    bool enableCMH = true;
    bool enableContHist = true;
    bool enableCorrHist = true;
};

struct EngineParameters {
    SearchParameters search{};
    EvaluationParameters eval{};
    TimeParameters time{};
    DebugParameters debug{};
};

} // namespace Boson

#endif // BOSON_ENGINE_PARAMETERS_HPP
