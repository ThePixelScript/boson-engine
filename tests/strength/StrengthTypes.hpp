#ifndef BOSON_STRENGTH_TYPES_HPP
#define BOSON_STRENGTH_TYPES_HPP

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include "config/EngineParameters.hpp"

namespace Boson {

enum class TerminationType : uint8_t {
    Checkmate,
    Stalemate,
    ThreefoldRepetition,
    FiftyMoveRule,
    InsufficientMaterial,
    Timeout,
    IllegalMove,
    EngineCrash,
    ProtocolError
};

[[nodiscard]] constexpr std::string_view terminationToString(TerminationType term) noexcept {
    switch (term) {
        case TerminationType::Checkmate:            return "Checkmate";
        case TerminationType::Stalemate:            return "Stalemate";
        case TerminationType::ThreefoldRepetition:  return "ThreefoldRepetition";
        case TerminationType::FiftyMoveRule:        return "FiftyMoveRule";
        case TerminationType::InsufficientMaterial: return "InsufficientMaterial";
        case TerminationType::Timeout:              return "Timeout";
        case TerminationType::IllegalMove:          return "IllegalMove";
        case TerminationType::EngineCrash:          return "EngineCrash";
        case TerminationType::ProtocolError:        return "ProtocolError";
    }
    return "Unknown";
}

enum class GameResult : uint8_t {
    WhiteWin,
    BlackWin,
    Draw
};

[[nodiscard]] constexpr std::string_view resultToString(GameResult res) noexcept {
    switch (res) {
        case GameResult::WhiteWin: return "1-0";
        case GameResult::BlackWin: return "0-1";
        case GameResult::Draw:     return "1/2-1/2";
    }
    return "*";
}

struct GameRecord {
    uint32_t gameId{0};
    std::string openingId{};
    std::string openingVersion{"1.0.0"};
    std::string whiteEngine{};
    std::string blackEngine{};
    GameResult result{GameResult::Draw};
    TerminationType termination{TerminationType::Stalemate};
    uint32_t plyCount{0};
    std::vector<std::string> moves{};
    std::string finalFen{};
    int64_t elapsedMs{0};
    std::string engineMetadata{};
    std::string pgn{};
};

struct MatchConfig {
    std::string engineA{"Boson-A"};
    std::string engineB{"Boson-B"};
    std::string engineACommit{"HEAD"};
    std::string engineBCommit{"HEAD"};
    std::string compiler{"MSVC"};
    std::string buildType{"Release"};
    std::string cpuArch{"x86_64"};
    int threads{1};
    size_t hashMb{64};
    int timeControlMs{50}; // fixed movetime in ms
    int fixedDepth{0};     // if > 0, overrides timeControlMs for deterministic fast runs
    std::string openingCorpusVersion{"1.0.0"};
    uint32_t totalGames{2};
    uint64_t seed{42};
    std::string paramsSnapshot{};
    EngineParameters paramsA{};
    EngineParameters paramsB{};
    uint32_t maxPlies{200}; // Safety cutoff to prevent runaway loops
    uint32_t gamesPerOpening{4};
};

enum class SPRTDecision : uint8_t {
    Continue,
    AcceptH1, // Pass
    AcceptH0  // Fail
};

[[nodiscard]] constexpr std::string_view sprtDecisionToString(SPRTDecision dec) noexcept {
    switch (dec) {
        case SPRTDecision::Continue: return "CONTINUE";
        case SPRTDecision::AcceptH1: return "PASS (H1 Accepted)";
        case SPRTDecision::AcceptH0: return "FAIL (H0 Accepted)";
    }
    return "UNKNOWN";
}

struct SPRTResult {
    double llr{0.0};
    double lowerBound{-2.944439}; // ln(beta / (1 - alpha)) for alpha=0.05, beta=0.05
    double upperBound{2.944439};  // ln((1 - beta) / alpha)
    double elo0{0.0};
    double elo1{10.0};
    SPRTDecision decision{SPRTDecision::Continue};
};

struct ConfidenceInterval {
    double observedScore{0.5};
    double rawWilsonLower{0.0};
    double rawWilsonUpper{1.0};
    double eloScoreLower{0.0};
    double eloScoreUpper{1.0};
    double deltaElo{0.0};
    double eloLower{0.0};
    double eloUpper{0.0};
};

using StatisticalEvaluation = ConfidenceInterval;

struct MatchStatistics {
    uint32_t wins{0};
    uint32_t draws{0};
    uint32_t losses{0};
    uint32_t totalGames{0};

    double score{0.5}; // p = (W + 0.5*D) / N
    double drawRate{0.0};
    double avgPly{0.0};
    double scorePercentage{50.0};
    double sampleVariance{0.0};
    double standardError{0.0};

    // Statistical output container fields
    double observedScore{0.5};
    double rawWilsonLower{0.0};
    double rawWilsonUpper{1.0};
    double eloScoreLower{0.0};
    double eloScoreUpper{1.0};
    double deltaElo{0.0};
    double eloLower{0.0};
    double eloUpper{0.0};

    // Phase 7-G specific decoupled fields
    double logisticElo{0.0};
    double ci95Margin{0.0};

    // Aliases for compatibility
    double scoreLow{0.0};
    double scoreHigh{1.0};
    double eloLow{0.0};
    double eloHigh{0.0};

    ConfidenceInterval ci{};

    SPRTResult sprt{};
};

struct MatchRecord {
    std::string schemaVersion{"1.0.0"};
    MatchConfig config{};
    MatchStatistics stats{};
    std::vector<GameRecord> games{};
    int64_t totalElapsedMs{0};
};

} // namespace Boson

#endif // BOSON_STRENGTH_TYPES_HPP
