#include "benchmark/BenchmarkRunner.hpp"
#include "benchmark/BenchmarkReporter.hpp"
#include "fen/FenParser.hpp"
#include "board/Position.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "evaluation/Evaluator.hpp"
#include "system/EngineInfo.hpp"
#include <chrono>
#include <sstream>
#include <iostream>

namespace Boson {

namespace {

struct CoutSilencer {
    std::streambuf* origBuf;
    std::ostringstream dummy;
    explicit CoutSilencer(bool silence)
        : origBuf(silence ? std::cout.rdbuf(dummy.rdbuf()) : nullptr) {}
    ~CoutSilencer() {
        if (origBuf) std::cout.rdbuf(origBuf);
    }
};

} // anonymous namespace

void BenchmarkRunner::resetSearchState(size_t hashSizeMb) noexcept {
    (void)hashSizeMb;
    Search::s_tt.clear();
    Search::clearCMH();
    Search::clearContHist();
    Evaluator::getCorrHist().clear();
    SearchController::getInstance().getStats().reset();
}

BenchmarkRunRecord BenchmarkRunner::run(const BenchmarkConfig& config) {
    BenchmarkRunRecord record;
    record.schemaVersion = "1.0.0";
    record.corpusVersion = std::string(BenchmarkCorpus::getVersion());
    record.engineName = std::string(EngineInfo::getName());
    record.engineVersion = std::string(EngineInfo::getVersion());
    record.compiler = EngineInfo::getCompiler();
    record.architecture = std::string(EngineInfo::getTargetArch());
    record.instructionSets = EngineInfo::getInstructionSets();
    record.mode = config.mode;
    record.hashSizeMb = config.hashSizeMb;
    record.threads = config.threads;
    record.serializedParameters = EngineInfo::serializeConfigSnapshot(SearchController::getInstance().getParams());

    resetSearchState(config.hashSizeMb);
    auto positions = BenchmarkCorpus::getPositions();

    for (const auto& bpos : positions) {
        int targetDepth = (config.overrideDepth > 0) ? config.overrideDepth : bpos.defaultDepth;

        if (config.mode == BenchmarkStateMode::Isolated) {
            resetSearchState(config.hashSizeMb);
        }

        auto parsed = FenParser::parse(bpos.fen);
        if (!parsed.has_value()) {
            std::cerr << "[ERROR] BenchmarkCorpus: Invalid FEN for " << bpos.id << "\n";
            continue;
        }

        Position pos = parsed.value();

        auto tStart = std::chrono::high_resolution_clock::now();
        int score = 0;

        {
            CoutSilencer silencer(config.silentSearch);
            SearchLimits limits;
            limits.depth = targetDepth;
            limits.clearTables = (config.mode == BenchmarkStateMode::Isolated);
            score = Search::runSearch(pos, limits);
        }

        auto tEnd = std::chrono::high_resolution_clock::now();
        int64_t elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();

        const auto& stats = SearchController::getInstance().getStats();

        PositionBenchmarkResult res;
        res.id = std::string(bpos.id);
        res.fen = std::string(bpos.fen);
        res.targetDepth = targetDepth;

        uint64_t totalNodes = stats.nodes + stats.qNodes;
        res.deterministic.totalNodes = totalNodes;
        res.deterministic.totalQNodes = stats.qNodes;
        res.deterministic.completedDepth = stats.completedDepth;
        res.deterministic.scoreCp = score;

        std::string bestMoveStr;
        if (stats.pvLine.count > 0) {
            bestMoveStr = stats.pvLine.moves[0].toString();
        } else {
            std::stringstream ss(stats.pvString);
            ss >> bestMoveStr;
        }
        res.deterministic.bestMoveUci = bestMoveStr;
        res.deterministic.pvString = stats.pvString;
        res.deterministic.ttHits = stats.ttHits;
        res.deterministic.ttCutoffs = stats.betaCutoffs;
        res.deterministic.betaCutoffs = stats.betaCutoffs;
        res.deterministic.nmpAttempts = stats.nullMoveAttempts;
        res.deterministic.nmpCutoffs = stats.nullMoveCutoffs;
        res.deterministic.lmrReductions = stats.lmrReducedNodes;
        res.deterministic.lmrResearches = stats.lmrResearches;
        res.deterministic.seeCalls = stats.seeInvocations;
        res.deterministic.seeRejections = (stats.seeInvocations > stats.qNodes) ? (stats.seeInvocations - stats.qNodes) : 0;

        res.environmental.elapsedMilliseconds = elapsedMs;
        res.environmental.nodesPerSecond = (elapsedMs > 0) ? (totalNodes * 1000) / elapsedMs : totalNodes;
        res.environmental.memoryAllocatedMb = config.hashSizeMb;

        record.positions.push_back(res);

        // Accumulate aggregate metrics
        record.aggregate.totalNodes += totalNodes;
        record.aggregate.totalQNodes += stats.qNodes;
        record.aggregate.elapsedMilliseconds += elapsedMs;
        record.aggregate.ttHits += stats.ttHits;
        record.aggregate.betaCutoffs += stats.betaCutoffs;
        record.aggregate.nmpAttempts += stats.nullMoveAttempts;
        record.aggregate.nmpCutoffs += stats.nullMoveCutoffs;
        record.aggregate.lmrReductions += stats.lmrReducedNodes;
        record.aggregate.lmrResearches += stats.lmrResearches;
        record.aggregate.seeCalls += stats.seeInvocations;
        record.aggregate.positionCount++;
    }

    if (record.aggregate.elapsedMilliseconds > 0) {
        record.aggregate.nodesPerSecond = (record.aggregate.totalNodes * 1000) / record.aggregate.elapsedMilliseconds;
    } else {
        record.aggregate.nodesPerSecond = record.aggregate.totalNodes;
    }

    if (config.printConsole) {
        BenchmarkReporter::printConsoleReport(record);
    }

    if (!config.jsonOutputFile.empty()) {
        BenchmarkReporter::writeJsonFile(config.jsonOutputFile, record);
    }

    return record;
}

} // namespace Boson
