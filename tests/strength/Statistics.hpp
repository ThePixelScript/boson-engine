#ifndef BOSON_STATISTICS_HPP
#define BOSON_STATISTICS_HPP

#include "strength/StrengthTypes.hpp"
#include <cmath>
#include <algorithm>

namespace Boson {

class Statistics {
public:
    [[nodiscard]] static double calculateScore(uint32_t wins, uint32_t draws, uint32_t totalGames) noexcept;
    [[nodiscard]] static double calculateSampleVariance(uint32_t wins, uint32_t draws, uint32_t losses, uint32_t totalGames, double p) noexcept;
    [[nodiscard]] static double calculateStandardError(double variance, uint32_t totalGames) noexcept;

    static void calculateWilsonInterval(double p, uint32_t totalGames, double& outRawLower, double& outRawUpper) noexcept;
    static void calculateScoreConfidenceInterval(double p, double se, uint32_t totalGames, double& outLow, double& outHigh) noexcept;

    [[nodiscard]] static ConfidenceInterval calculateConfidenceInterval(double p, uint32_t totalGames) noexcept;

    [[nodiscard]] static double eloFromScore(double p, double eps = 1e-6) noexcept;
    static void calculateEloConfidenceInterval(double scoreLow, double scoreHigh, double& outEloLow, double& outEloHigh) noexcept;

    [[nodiscard]] static double calculateDrawRate(uint32_t draws, uint32_t totalGames) noexcept;
    [[nodiscard]] static double calculateLogisticElo(double p, double eps = 1e-6) noexcept;
    [[nodiscard]] static double calculateCI95Margin(double p, uint32_t totalGames, double eps = 1e-6) noexcept;

    [[nodiscard]] static SPRTResult evaluateSPRT(uint32_t wins, uint32_t draws, uint32_t losses, double elo0 = 0.0, double elo1 = 10.0, double alpha = 0.05, double beta = 0.05) noexcept;

    [[nodiscard]] static MatchStatistics computeFixedSampleStatistics(uint32_t wins, uint32_t draws, uint32_t losses) noexcept;
    [[nodiscard]] static MatchStatistics computeStatistics(uint32_t wins, uint32_t draws, uint32_t losses, double elo0 = 0.0, double elo1 = 10.0, double alpha = 0.05, double beta = 0.05) noexcept;
};

} // namespace Boson

#endif // BOSON_STATISTICS_HPP
