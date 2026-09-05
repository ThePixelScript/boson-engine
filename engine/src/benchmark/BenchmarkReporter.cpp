#include "benchmark/BenchmarkReporter.hpp"
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace Boson {

void BenchmarkReporter::printConsoleReport(const BenchmarkRunRecord& record, std::ostream& out) {
    out << "\n=================================================================\n";
    out << "===               BOSON BENCHMARK REPORT                      ===\n";
    out << "=================================================================\n";
    out << "[Engine]\n";
    out << "  Version           : " << record.engineName << " " << record.engineVersion << "\n";
    out << "  Compiler          : " << record.compiler << "\n";
    out << "  Target Arch / ISA : " << record.architecture << " [" << record.instructionSets << "]\n";
    out << "  Execution Mode    : " << (record.mode == BenchmarkStateMode::Isolated ? "Isolated (Clean State)" : "Persistent (Warm State)") << "\n";
    out << "  Hash / Threads    : " << record.hashSizeMb << " MB / " << record.threads << " thread(s)\n";

    double ttHitRate = (record.aggregate.totalNodes > 0)
        ? (static_cast<double>(record.aggregate.ttHits) * 100.0 / record.aggregate.totalNodes) : 0.0;
    double betaCutoffRate = (record.aggregate.totalNodes > 0)
        ? (static_cast<double>(record.aggregate.betaCutoffs) * 100.0 / record.aggregate.totalNodes) : 0.0;

    out << "\n[Search & Evaluation Aggregates]\n";
    out << "  Positions Tested  : " << record.aggregate.positionCount << "\n";
    out << "  Total Nodes       : " << record.aggregate.totalNodes 
        << " (Base: " << (record.aggregate.totalNodes - record.aggregate.totalQNodes) 
        << ", QNodes: " << record.aggregate.totalQNodes << ")\n";
    out << "  Elapsed Time      : " << record.aggregate.elapsedMilliseconds << " ms\n";
    out << "  Aggregate NPS     : " << record.aggregate.nodesPerSecond << "\n";
    out << "  TT Hit Rate       : " << std::fixed << std::setprecision(2) << ttHitRate << "% (" << record.aggregate.ttHits << " hits)\n";
    out << "  Beta Cutoff Rate  : " << std::fixed << std::setprecision(2) << betaCutoffRate << "% (" << record.aggregate.betaCutoffs << " cutoffs)\n";
    out << "  Null Move Cutoffs : " << record.aggregate.nmpCutoffs << " / " << record.aggregate.nmpAttempts << " attempts\n";
    out << "  LMR Reductions    : " << record.aggregate.lmrReductions << " (Researches: " << record.aggregate.lmrResearches << ")\n";
    out << "  SEE Invocations   : " << record.aggregate.seeCalls << "\n";

    out << "\n[Per-Position Breakdown Table]\n";
    out << "+-------------------------+-------+----------+---------+------------+-----------+-----------+----------+\n";
    out << "| Position ID             | Depth | Nodes    | QNodes  | Score (cp) | Best Move | Time (ms) | NPS      |\n";
    out << "+-------------------------+-------+----------+---------+------------+-----------+-----------+----------+\n";

    for (const auto& pos : record.positions) {
        out << "| " << std::left << std::setw(23) << pos.id.substr(0, 23)
            << " | " << std::right << std::setw(5) << pos.targetDepth
            << " | " << std::setw(8) << pos.deterministic.totalNodes
            << " | " << std::setw(7) << pos.deterministic.totalQNodes
            << " | " << std::setw(10) << pos.deterministic.scoreCp
            << " | " << std::setw(9) << pos.deterministic.bestMoveUci
            << " | " << std::setw(9) << pos.environmental.elapsedMilliseconds
            << " | " << std::setw(8) << pos.environmental.nodesPerSecond
            << " |\n";
    }

    out << "+-------------------------+-------+----------+---------+------------+-----------+-----------+----------+\n";
    out << "| AGGREGATE TOTAL / AVG   |   -   | " 
        << std::right << std::setw(8) << record.aggregate.totalNodes << " | "
        << std::setw(7) << record.aggregate.totalQNodes << " | "
        << std::setw(10) << "-" << " | "
        << std::setw(9) << "-" << " | "
        << std::setw(9) << record.aggregate.elapsedMilliseconds << " | "
        << std::setw(8) << record.aggregate.nodesPerSecond << " |\n";
    out << "+-------------------------+-------+----------+---------+------------+-----------+-----------+----------+\n";
    out << "=================================================================\n";
}

std::string BenchmarkReporter::serializeJson(const BenchmarkRunRecord& record) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"schemaVersion\": \"" << record.schemaVersion << "\",\n";
    json << "  \"corpusVersion\": \"" << record.corpusVersion << "\",\n";
    json << "  \"engineName\": \"" << record.engineName << "\",\n";
    json << "  \"engineVersion\": \"" << record.engineVersion << "\",\n";
    json << "  \"compiler\": \"" << record.compiler << "\",\n";
    json << "  \"architecture\": \"" << record.architecture << "\",\n";
    json << "  \"instructionSets\": \"" << record.instructionSets << "\",\n";
    json << "  \"mode\": \"" << (record.mode == BenchmarkStateMode::Isolated ? "Isolated" : "Persistent") << "\",\n";
    json << "  \"hashSizeMb\": " << record.hashSizeMb << ",\n";
    json << "  \"threads\": " << record.threads << ",\n";

    json << "  \"positions\": [\n";
    for (size_t i = 0; i < record.positions.size(); ++i) {
        const auto& pos = record.positions[i];
        json << "    {\n";
        json << "      \"id\": \"" << pos.id << "\",\n";
        json << "      \"fen\": \"" << pos.fen << "\",\n";
        json << "      \"targetDepth\": " << pos.targetDepth << ",\n";
        json << "      \"completedDepth\": " << pos.deterministic.completedDepth << ",\n";
        json << "      \"totalNodes\": " << pos.deterministic.totalNodes << ",\n";
        json << "      \"totalQNodes\": " << pos.deterministic.totalQNodes << ",\n";
        json << "      \"scoreCp\": " << pos.deterministic.scoreCp << ",\n";
        json << "      \"bestMoveUci\": \"" << pos.deterministic.bestMoveUci << "\",\n";
        json << "      \"ttHits\": " << pos.deterministic.ttHits << ",\n";
        json << "      \"betaCutoffs\": " << pos.deterministic.betaCutoffs << ",\n";
        json << "      \"nmpAttempts\": " << pos.deterministic.nmpAttempts << ",\n";
        json << "      \"nmpCutoffs\": " << pos.deterministic.nmpCutoffs << ",\n";
        json << "      \"lmrReductions\": " << pos.deterministic.lmrReductions << ",\n";
        json << "      \"lmrResearches\": " << pos.deterministic.lmrResearches << ",\n";
        json << "      \"seeCalls\": " << pos.deterministic.seeCalls << ",\n";
        json << "      \"elapsedMilliseconds\": " << pos.environmental.elapsedMilliseconds << ",\n";
        json << "      \"nodesPerSecond\": " << pos.environmental.nodesPerSecond << "\n";
        json << "    }" << (i + 1 < record.positions.size() ? "," : "") << "\n";
    }
    json << "  ],\n";

    json << "  \"aggregate\": {\n";
    json << "    \"positionCount\": " << record.aggregate.positionCount << ",\n";
    json << "    \"totalNodes\": " << record.aggregate.totalNodes << ",\n";
    json << "    \"totalQNodes\": " << record.aggregate.totalQNodes << ",\n";
    json << "    \"elapsedMilliseconds\": " << record.aggregate.elapsedMilliseconds << ",\n";
    json << "    \"nodesPerSecond\": " << record.aggregate.nodesPerSecond << ",\n";
    json << "    \"ttHits\": " << record.aggregate.ttHits << ",\n";
    json << "    \"betaCutoffs\": " << record.aggregate.betaCutoffs << "\n";
    json << "  }\n";
    json << "}\n";

    return json.str();
}

bool BenchmarkReporter::writeJsonFile(const std::string& filepath, const BenchmarkRunRecord& record) {
    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << serializeJson(record);
        return true;
    } catch (...) {
        return false;
    }
}

bool BenchmarkReporter::parseJsonParity(const std::string& jsonStr, std::string& outSchemaVersion, std::string& outCorpusVersion, uint64_t& outTotalNodes, size_t& outPosCount) {
    // Lightweight string parsing to verify JSON serialization round-trip
    auto findValue = [&](std::string_view key) -> std::string {
        std::string searchKey = "\"" + std::string(key) + "\":";
        size_t pos = jsonStr.find(searchKey);
        if (pos == std::string::npos) return "";
        pos += searchKey.size();
        while (pos < jsonStr.size() && (jsonStr[pos] == ' ' || jsonStr[pos] == '\t' || jsonStr[pos] == '\"')) pos++;
        size_t end = pos;
        while (end < jsonStr.size() && jsonStr[end] != '\"' && jsonStr[end] != ',' && jsonStr[end] != '\r' && jsonStr[end] != '\n' && jsonStr[end] != '}') end++;
        return jsonStr.substr(pos, end - pos);
    };

    outSchemaVersion = findValue("schemaVersion");
    outCorpusVersion = findValue("corpusVersion");

    std::string totalNodesStr;
    size_t aggPos = jsonStr.find("\"aggregate\":");
    if (aggPos != std::string::npos) {
        size_t tnPos = jsonStr.find("\"totalNodes\":", aggPos);
        if (tnPos != std::string::npos) {
            tnPos += 13;
            while (tnPos < jsonStr.size() && (jsonStr[tnPos] == ' ' || jsonStr[tnPos] == '\t')) tnPos++;
            size_t end = tnPos;
            while (end < jsonStr.size() && (jsonStr[end] >= '0' && jsonStr[end] <= '9')) end++;
            totalNodesStr = jsonStr.substr(tnPos, end - tnPos);
        }
    }

    if (outSchemaVersion.empty() || outCorpusVersion.empty() || totalNodesStr.empty()) {
        return false;
    }

    try {
        outTotalNodes = std::stoull(totalNodesStr);
    } catch (...) {
        return false;
    }

    // Count positions by counting "id": occurrences
    size_t count = 0;
    size_t pos = 0;
    while ((pos = jsonStr.find("\"id\":", pos)) != std::string::npos) {
        count++;
        pos += 5;
    }
    outPosCount = count;

    return true;
}

} // namespace Boson
