#include "strength/Statistics.hpp"
#include <cassert>
#include <cmath>
#include <algorithm>

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

void Statistics::calculateWilsonInterval(double p, uint32_t totalGames, double& outRawLower, double& outRawUpper) noexcept {
    if (totalGames == 0) {
        outRawLower = 0.0;
        outRawUpper = 1.0;
        return;
    }
    const double N = static_cast<double>(totalGames);
    const double z = 1.959963984540054;
    const double zSq = z * z;
    const double denominator = 1.0 + (zSq / N);
    const double center = (p + (zSq / (2.0 * N))) / denominator;
    const double radicand = (p * (1.0 - p) / N) + (zSq / (4.0 * N * N));
    const double halfWidth = (z * std::sqrt(std::max(0.0, radicand))) / denominator;

    outRawLower = std::max(0.0, center - halfWidth);
    outRawUpper = std::min(1.0, center + halfWidth);
}

void Statistics::calculateScoreConfidenceInterval(double p, double se, uint32_t totalGames, double& outLow, double& outHigh) noexcept {
    (void)se;
    calculateWilsonInterval(p, totalGames, outLow, outHigh);
}

ConfidenceInterval Statistics::calculateConfidenceInterval(double p, uint32_t totalGames) noexcept {
    ConfidenceInterval ci;
    ci.observedScore = p;

    if (totalGames == 0) {
        ci.rawWilsonLower = 0.0;
        ci.rawWilsonUpper = 1.0;
        ci.eloScoreLower = 0.0;
        ci.eloScoreUpper = 1.0;
        ci.deltaElo = 0.0;
        ci.eloLower = 0.0;
        ci.eloUpper = 0.0;
        return ci;
    }

    const double N = static_cast<double>(totalGames);
    const double z = 1.959963984540054;
    const double zSq = z * z;
    const double denominator = 1.0 + (zSq / N);
    const double center = (p + (zSq / (2.0 * N))) / denominator;
    const double radicand = (p * (1.0 - p) / N) + (zSq / (4.0 * N * N));
    const double halfWidth = (z * std::sqrt(std::max(0.0, radicand))) / denominator;

    ci.rawWilsonLower = std::max(0.0, center - halfWidth);
    ci.rawWilsonUpper = std::min(1.0, center + halfWidth);

    // Decouple numerical domain guard from sample size N using fixed constant
    constexpr double kLogisticEpsilon = 1e-6;
    ci.eloScoreLower = std::clamp(ci.rawWilsonLower, kLogisticEpsilon, 1.0 - kLogisticEpsilon);
    ci.eloScoreUpper = std::clamp(ci.rawWilsonUpper, kLogisticEpsilon, 1.0 - kLogisticEpsilon);
    const double p_clamped = std::clamp(p, kLogisticEpsilon, 1.0 - kLogisticEpsilon);

    if (std::abs(p - 0.5) < 1e-12) {
        ci.deltaElo = 0.0;
    } else {
        ci.deltaElo = 400.0 * std::log10(p_clamped / (1.0 - p_clamped));
    }

    ci.eloLower = 400.0 * std::log10(ci.eloScoreLower / (1.0 - ci.eloScoreLower));
    ci.eloUpper = 400.0 * std::log10(ci.eloScoreUpper / (1.0 - ci.eloScoreUpper));

    assert(std::isfinite(ci.observedScore));
    assert(std::isfinite(ci.rawWilsonLower));
    assert(std::isfinite(ci.rawWilsonUpper));
    assert(std::isfinite(ci.eloScoreLower));
    assert(std::isfinite(ci.eloScoreUpper));
    assert(std::isfinite(ci.deltaElo));
    assert(std::isfinite(ci.eloLower));
    assert(std::isfinite(ci.eloUpper));

    return ci;
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

double Statistics::calculateDrawRate(uint32_t draws, uint32_t totalGames) noexcept {
    if (totalGames == 0) return 0.0;
    return static_cast<double>(draws) / static_cast<double>(totalGames);
}

double Statistics::calculateLogisticElo(double p, double eps) noexcept {
    double p_clamped = std::clamp(p, eps, 1.0 - eps);
    if (std::abs(p - 0.5) < 1e-12) return 0.0;
    return -400.0 * std::log10(1.0 / p_clamped - 1.0);
}

double Statistics::calculateCI95Margin(double p, uint32_t totalGames, double eps) noexcept {
    if (totalGames == 0) return 0.0;
    double p_clamped = std::clamp(p, eps, 1.0 - eps);
    double N = static_cast<double>(totalGames);
    double se = std::sqrt(p_clamped * (1.0 - p_clamped) / N);
    double deriv = 400.0 / (std::log(10.0) * p_clamped * (1.0 - p_clamped));
    return 1.96 * se * deriv;
}

MatchStatistics Statistics::computeFixedSampleStatistics(uint32_t wins, uint32_t draws, uint32_t losses) noexcept {
    MatchStatistics stats;
    stats.wins = wins;
    stats.draws = draws;
    stats.losses = losses;
    stats.totalGames = wins + draws + losses;

    stats.score = calculateScore(wins, draws, stats.totalGames);
    stats.drawRate = calculateDrawRate(draws, stats.totalGames);
    stats.scorePercentage = stats.score * 100.0;
    stats.sampleVariance = calculateSampleVariance(wins, draws, losses, stats.totalGames, stats.score);
    stats.standardError = calculateStandardError(stats.sampleVariance, stats.totalGames);

    stats.logisticElo = calculateLogisticElo(stats.score);
    stats.ci95Margin = calculateCI95Margin(stats.score, stats.totalGames);

    stats.ci = calculateConfidenceInterval(stats.score, stats.totalGames);
    stats.observedScore = stats.ci.observedScore;
    stats.rawWilsonLower = stats.ci.rawWilsonLower;
    stats.rawWilsonUpper = stats.ci.rawWilsonUpper;
    stats.eloScoreLower = stats.ci.eloScoreLower;
    stats.eloScoreUpper = stats.ci.eloScoreUpper;
    stats.deltaElo = stats.ci.deltaElo;

    stats.eloLower = stats.logisticElo - stats.ci95Margin;
    stats.eloUpper = stats.logisticElo + stats.ci95Margin;
    stats.eloLow = stats.eloLower;
    stats.eloHigh = stats.eloUpper;
    stats.scoreLow = stats.rawWilsonLower;
    stats.scoreHigh = stats.rawWilsonUpper;

    return stats;
}

MatchStatistics Statistics::computeStatistics(uint32_t wins, uint32_t draws, uint32_t losses, double elo0, double elo1, double alpha, double beta) noexcept {
    MatchStatistics stats = computeFixedSampleStatistics(wins, draws, losses);
    stats.sprt = evaluateSPRT(wins, draws, losses, elo0, elo1, alpha, beta);
    return stats;
}

} // namespace Boson
