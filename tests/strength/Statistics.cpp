#include "strength/Statistics.hpp"

namespace Boson {

double Statistics::calculateScore(uint32_t wins, uint32_t draws, uint32_t totalGames) noexcept {
    if (totalGames == 0) return 0.5;
    return (static_cast<double>(wins) + 0.5 * static_cast<double>(draws)) / static_cast<double>(totalGames);
}

double Statistics::calculateSampleVariance(uint32_t wins, uint32_t draws, uint32_t losses, uint32_t totalGames, double p) noexcept {
    if (totalGames <= 1) return 0.0;
    double w = static_cast<double>(wins);
    double d = static_cast<double>(draws);
    double l = static_cast<double>(losses);
    double devW = 1.0 - p;
    double devD = 0.5 - p;
    double devL = 0.0 - p;
    double sumSq = w * devW * devW + d * devD * devD + l * devL * devL;
    double var = sumSq / static_cast<double>(totalGames - 1);
    return (var > 0.0) ? var : 0.0;
}

double Statistics::calculateStandardError(double variance, uint32_t totalGames) noexcept {
    if (totalGames == 0 || variance <= 0.0) return 0.0;
    return std::sqrt(variance / static_cast<double>(totalGames));
}

void Statistics::calculateScoreConfidenceInterval(double p, double se, uint32_t totalGames, double& outLow, double& outHigh) noexcept {
    if (totalGames == 0) {
        outLow = 0.0;
        outHigh = 1.0;
        return;
    }
    double eps = 1.0 / (2.0 * static_cast<double>(totalGames));
    outLow = std::clamp(p - 1.96 * se, eps, 1.0 - eps);
    outHigh = std::clamp(p + 1.96 * se, eps, 1.0 - eps);
    if (outLow > outHigh) {
        std::swap(outLow, outHigh);
    }
}

double Statistics::eloFromScore(double p, double eps) noexcept {
    if (p <= 0.0) p = eps;
    if (p >= 1.0) p = 1.0 - eps;
    if (std::abs(p - 0.5) < 1e-12) return 0.0;
    return 400.0 * std::log10(p / (1.0 - p));
}

void Statistics::calculateEloConfidenceInterval(double scoreLow, double scoreHigh, double& outEloLow, double& outEloHigh) noexcept {
    outEloLow = eloFromScore(scoreLow);
    outEloHigh = eloFromScore(scoreHigh);
}

SPRTResult Statistics::evaluateSPRT(uint32_t wins, uint32_t draws, uint32_t losses, double elo0, double elo1, double alpha, double beta) noexcept {
    SPRTResult res;
    res.elo0 = elo0;
    res.elo1 = elo1;
    res.lowerBound = std::log(beta / (1.0 - alpha));
    res.upperBound = std::log((1.0 - beta) / alpha);

    double p0 = 1.0 / (1.0 + std::pow(10.0, -elo0 / 400.0));
    double p1 = 1.0 / (1.0 + std::pow(10.0, -elo1 / 400.0));

    double s = static_cast<double>(wins) + 0.5 * static_cast<double>(draws);
    double c = static_cast<double>(losses) + 0.5 * static_cast<double>(draws);

    res.llr = s * std::log(p1 / p0) + c * std::log((1.0 - p1) / (1.0 - p0));

    if (res.llr >= res.upperBound) {
        res.decision = SPRTDecision::AcceptH1;
    } else if (res.llr <= res.lowerBound) {
        res.decision = SPRTDecision::AcceptH0;
    } else {
        res.decision = SPRTDecision::Continue;
    }

    return res;
}

MatchStatistics Statistics::computeStatistics(uint32_t wins, uint32_t draws, uint32_t losses, double elo0, double elo1, double alpha, double beta) noexcept {
    MatchStatistics stats;
    stats.wins = wins;
    stats.draws = draws;
    stats.losses = losses;
    stats.totalGames = wins + draws + losses;

    stats.score = calculateScore(wins, draws, stats.totalGames);
    stats.scorePercentage = stats.score * 100.0;
    stats.sampleVariance = calculateSampleVariance(wins, draws, losses, stats.totalGames, stats.score);
    stats.standardError = calculateStandardError(stats.sampleVariance, stats.totalGames);

    calculateScoreConfidenceInterval(stats.score, stats.standardError, stats.totalGames, stats.scoreLow, stats.scoreHigh);

    stats.deltaElo = eloFromScore(stats.score);
    calculateEloConfidenceInterval(stats.scoreLow, stats.scoreHigh, stats.eloLow, stats.eloHigh);

    stats.sprt = evaluateSPRT(wins, draws, losses, elo0, elo1, alpha, beta);

    return stats;
}

} // namespace Boson
