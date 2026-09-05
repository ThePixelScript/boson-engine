#include "strength/StrengthReporter.hpp"
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace Boson {

void StrengthReporter::printConsoleReport(const MatchRecord& record, std::ostream& out) {
    const auto& cfg = record.config;
    const auto& st = record.stats;

    out << "\n================================================================================\n";
    out << "                    BOSON STRENGTH & ELO MATCH REPORT\n";
    out << "================================================================================\n";
    out << "[Match Configuration]\n";
    out << "  Engine A:         " << cfg.engineA << "\n";
    out << "  Engine B:         " << cfg.engineB << "\n";
    out << "  Total Games:      " << st.totalGames << " (" << (st.totalGames + 1) / 2 << " pairings)\n";
    if (cfg.fixedDepth > 0) {
        out << "  Search Mode:      Fixed Depth " << cfg.fixedDepth << "\n";
    } else {
        out << "  Time Control:     " << cfg.timeControlMs << " ms / move\n";
    }
    out << "  Opening Corpus:   v" << cfg.openingCorpusVersion << "\n";
    out << "  Hardware/Env:     " << cfg.cpuArch << " | " << cfg.compiler << " (" << cfg.buildType << ")\n";

    out << "\n--------------------------------------------------------------------------------\n";
    out << "[Match Outcome Summary]\n";
    out << std::fixed << std::setprecision(1);
    out << "  " << cfg.engineA << " Score:   " << (st.wins + 0.5 * st.draws) << " / " << st.totalGames 
        << " (" << st.scorePercentage << "%)\n";
    out << "  Outcomes:         " << st.wins << " Wins (+" << st.wins << "), "
        << st.draws << " Draws (=" << st.draws << "), "
        << st.losses << " Losses (-" << st.losses << ")\n";
    out << std::setprecision(4);
    out << "  Sample Variance:  " << st.sampleVariance << " | Std Error: " << st.standardError << "\n";
    out << std::setprecision(1);
    out << "  Score 95% CI:     [" << (st.scoreLow * 100.0) << "%, " << (st.scoreHigh * 100.0) << "%]\n";
    out << "  Delta Elo:        " << (st.deltaElo >= 0 ? "+" : "") << st.deltaElo << " Elo\n";
    out << "  Elo 95% CI:       [" << (st.eloLow >= 0 ? "+" : "") << st.eloLow << ", "
        << (st.eloHigh >= 0 ? "+" : "") << st.eloHigh << "] Elo\n";
    out << std::setprecision(2);
    out << "  SPRT Status:      " << sprtDecisionToString(st.sprt.decision)
        << " (LLR: " << st.sprt.llr << " [" << st.sprt.lowerBound << ", " << st.sprt.upperBound << "])\n";

    out << "\n--------------------------------------------------------------------------------\n";
    out << "[Game-by-Game Log]\n";
    out << "Game   White            Black            Opening   Plies   Result   Termination\n";
    out << "--------------------------------------------------------------------------------\n";
    for (const auto& g : record.games) {
        out << std::setw(4) << g.gameId << "   "
            << std::left << std::setw(16) << g.whiteEngine
            << std::left << std::setw(16) << g.blackEngine
            << std::left << std::setw(10) << g.openingId
            << std::right << std::setw(5) << g.plyCount << "   "
            << std::left << std::setw(8) << resultToString(g.result)
            << std::left << terminationToString(g.termination) << "\n";
    }
    out << "================================================================================\n" << std::endl;
}

std::string StrengthReporter::serializeJson(const MatchRecord& record) {
    const auto& cfg = record.config;
    const auto& st = record.stats;

    std::ostringstream json;
    json << std::fixed;

    json << "{\n";
    json << "  \"schemaVersion\": \"" << record.schemaVersion << "\",\n";

    json << "  \"matchConfig\": {\n";
    json << "    \"engineA\": \"" << cfg.engineA << "\",\n";
    json << "    \"engineB\": \"" << cfg.engineB << "\",\n";
    json << "    \"compiler\": \"" << cfg.compiler << "\",\n";
    json << "    \"buildType\": \"" << cfg.buildType << "\",\n";
    json << "    \"cpuArch\": \"" << cfg.cpuArch << "\",\n";
    json << "    \"threads\": " << cfg.threads << ",\n";
    json << "    \"hashMb\": " << cfg.hashMb << ",\n";
    json << "    \"timeControlMs\": " << cfg.timeControlMs << ",\n";
    json << "    \"fixedDepth\": " << cfg.fixedDepth << ",\n";
    json << "    \"openingCorpusVersion\": \"" << cfg.openingCorpusVersion << "\",\n";
    json << "    \"totalGames\": " << cfg.totalGames << "\n";
    json << "  },\n";

    json << "  \"statistics\": {\n";
    json << "    \"wins\": " << st.wins << ",\n";
    json << "    \"draws\": " << st.draws << ",\n";
    json << "    \"losses\": " << st.losses << ",\n";
    json << "    \"totalGames\": " << st.totalGames << ",\n";
    json << std::setprecision(4);
    json << "    \"score\": " << st.score << ",\n";
    json << std::setprecision(2);
    json << "    \"scorePercentage\": " << st.scorePercentage << ",\n";
    json << std::setprecision(6);
    json << "    \"sampleVariance\": " << st.sampleVariance << ",\n";
    json << "    \"standardError\": " << st.standardError << ",\n";
    json << "    \"scoreLow\": " << st.scoreLow << ",\n";
    json << "    \"scoreHigh\": " << st.scoreHigh << ",\n";
    json << std::setprecision(2);
    json << "    \"deltaElo\": " << st.deltaElo << ",\n";
    json << "    \"eloLow\": " << st.eloLow << ",\n";
    json << "    \"eloHigh\": " << st.eloHigh << ",\n";
    json << "    \"sprt\": {\n";
    json << "      \"llr\": " << st.sprt.llr << ",\n";
    json << "      \"lowerBound\": " << st.sprt.lowerBound << ",\n";
    json << "      \"upperBound\": " << st.sprt.upperBound << ",\n";
    json << "      \"decision\": \"" << sprtDecisionToString(st.sprt.decision) << "\"\n";
    json << "    }\n";
    json << "  },\n";

    json << "  \"games\": [\n";
    for (size_t i = 0; i < record.games.size(); ++i) {
        const auto& g = record.games[i];
        json << "    {\n";
        json << "      \"gameId\": " << g.gameId << ",\n";
        json << "      \"whiteEngine\": \"" << g.whiteEngine << "\",\n";
        json << "      \"blackEngine\": \"" << g.blackEngine << "\",\n";
        json << "      \"openingId\": \"" << g.openingId << "\",\n";
        json << "      \"result\": \"" << resultToString(g.result) << "\",\n";
        json << "      \"termination\": \"" << terminationToString(g.termination) << "\",\n";
        json << "      \"plyCount\": " << g.plyCount << ",\n";
        json << "      \"elapsedMs\": " << g.elapsedMs << "\n";
        json << "    }" << (i + 1 < record.games.size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    json << "  \"totalElapsedMs\": " << record.totalElapsedMs << "\n";
    json << "}\n";

    return json.str();
}

bool StrengthReporter::writeJsonFile(const std::string& filepath, const MatchRecord& record) {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream ofs(p);
        if (!ofs.is_open()) return false;
        ofs << serializeJson(record);
        return true;
    } catch (...) {
        return false;
    }
}

bool StrengthReporter::parseJsonParity(const std::string& jsonStr, std::string& outSchemaVersion, uint32_t& outTotalGames, double& outScore, double& outDeltaElo) {
    auto findValue = [&](std::string_view key, size_t startPos = 0) -> std::string {
        std::string searchKey = "\"" + std::string(key) + "\":";
        size_t pos = jsonStr.find(searchKey, startPos);
        if (pos == std::string::npos) return "";
        pos += searchKey.size();
        while (pos < jsonStr.size() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t' || jsonStr[pos] == '\"')) pos++;
        size_t end = pos;
        while (end < jsonStr.size() && jsonStr[end] != '\"' && jsonStr[end] != ',' && jsonStr[end] != '\r' && jsonStr[end] != '\n' && jsonStr[end] != '}') end++;
        return jsonStr.substr(pos, end - pos);
    };

    outSchemaVersion = findValue("schemaVersion");

    size_t statsPos = jsonStr.find("\"statistics\":");
    if (statsPos == std::string::npos) return false;

    std::string totalGamesStr = findValue("totalGames", statsPos);
    std::string scoreStr = findValue("score", statsPos);
    std::string deltaEloStr = findValue("deltaElo", statsPos);

    if (outSchemaVersion.empty() || totalGamesStr.empty() || scoreStr.empty() || deltaEloStr.empty()) {
        return false;
    }

    try {
        outTotalGames = static_cast<uint32_t>(std::stoul(totalGamesStr));
        outScore = std::stod(scoreStr);
        outDeltaElo = std::stod(deltaEloStr);
    } catch (...) {
        return false;
    }

    return true;
}

} // namespace Boson
