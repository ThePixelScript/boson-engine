#ifndef BOSON_SEARCH_CONTROLLER_HPP
#define BOSON_SEARCH_CONTROLLER_HPP

#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include "SearchLimits.hpp"
#include "SearchStatistics.hpp"
#include "TimeManager.hpp"
#include "board/Position.hpp"
#include "config/EngineParameters.hpp"
#include "system/EngineInfo.hpp"

namespace Boson {

class SearchController {
public:
    static SearchController& getInstance() noexcept {
        static SearchController instance;
        return instance;
    }

    void initSearch(const SearchLimits& limits, const Position& pos) noexcept {
        m_stopToken.store(false, std::memory_order_relaxed);
        m_stats.reset();
        m_limits = limits;
        m_timeManager.calculateLimits(limits, pos.getSideToMove(), m_params.time);
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    void checkTime() noexcept {
        if (!m_timeManager.hasTimeLimit()) return;

        auto current = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current - m_startTime).count();

        if (elapsed >= m_timeManager.getHardLimit()) {
            m_stopToken.store(true, std::memory_order_relaxed);
            m_stats.stopReason = StopReason::HardTimeLimit;
        }
    }

    [[nodiscard]] bool shouldStop() const noexcept {
        return m_stopToken.load(std::memory_order_relaxed);
    }

    void requestStop(StopReason reason) noexcept {
        m_stopToken.store(true, std::memory_order_relaxed);
        m_stats.stopReason = reason;
    }

    [[nodiscard]] int64_t getElapsedTimeMs() const noexcept {
        auto current = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(current - m_startTime).count();
    }

    [[nodiscard]] const TimeManager& getTimeManager() const noexcept { return m_timeManager; }
    [[nodiscard]] SearchStatistics& getStats() noexcept { return m_stats; }
    [[nodiscard]] const SearchLimits& getLimits() const noexcept { return m_limits; }

    [[nodiscard]] const EngineParameters& getParams() const noexcept { return m_params; }
    [[nodiscard]] EngineParameters& getMutableParams() noexcept { return m_params; }
    void setParams(const EngineParameters& params) noexcept { m_params = params; }

    void printBenchmarkReport(int targetDepth, size_t hashSizeMb = 16, int threads = 1) const noexcept {
        uint64_t totalNodes = m_stats.nodes + m_stats.qNodes;
        int64_t elapsed = m_stats.elapsedTimeMs;
        uint64_t nps = elapsed > 0 ? (totalNodes * 1000) / elapsed : totalNodes;
        double ttHitRate = (totalNodes > 0) ? (static_cast<double>(m_stats.ttHits) * 100.0 / totalNodes) : 0.0;
        double betaCutoffRate = (totalNodes > 0) ? (static_cast<double>(m_stats.betaCutoffs) * 100.0 / totalNodes) : 0.0;

        std::cout << "\n=================================================================\n";
        std::cout << "===               BOSON BENCHMARK REPORT                      ===\n";
        std::cout << "=================================================================\n";
        std::cout << "[Engine]\n";
        std::cout << "  Version           : " << EngineInfo::getName() << " " << EngineInfo::getVersion() 
                  << " (" << EngineInfo::getBuildType() << ")\n";
        std::cout << "  Compiler          : " << EngineInfo::getCompiler() << "\n";
        std::cout << "  Target Arch / ISA : " << EngineInfo::getTargetArch() << " [" << EngineInfo::getInstructionSets() << "]\n";

        std::cout << "\n[Search]\n";
        std::cout << "  Target Depth      : " << targetDepth << "\n";
        std::cout << "  Completed Depth   : " << m_stats.completedDepth << "\n";
        std::cout << "  Nodes             : " << totalNodes << " (Base: " << m_stats.nodes << ", QNodes: " << m_stats.qNodes << ")\n";
        std::cout << "  NPS               : " << nps << "\n";
        std::cout << "  TT Hit Rate       : " << std::fixed << std::setprecision(2) << ttHitRate << "% (" << m_stats.ttHits << " hits)\n";
        std::cout << "  Beta Cutoff Rate  : " << std::fixed << std::setprecision(2) << betaCutoffRate << "% (" << m_stats.betaCutoffs << " cutoffs)\n";

        std::cout << "\n[Evaluation]\n";
        std::cout << "  Static Eval Calls : " << m_stats.staticEvalCalls << "\n";
        std::cout << "  SEE Invocations   : " << m_stats.seeInvocations << "\n";
        std::cout << "  CorrHist Updates  : " << m_stats.corrUpdates << "\n";
        std::cout << "  CorrHist Applied  : " << m_stats.corrApplied << " (+: " << m_stats.corrPositive << ", -: " << m_stats.corrNegative << ")\n";
        std::cout << "  CorrHist Total Mag: " << m_stats.corrTotalMagnitude << " cp\n";

        std::cout << "\n[System]\n";
        std::cout << "  Transposition Size: " << hashSizeMb << " MB\n";
        std::cout << "  Concurrent Threads: " << threads << "\n";
        std::cout << "  Elapsed Time      : " << elapsed << " ms\n";
        std::cout << "=================================================================\n";
    }

private:
    SearchController() = default;

    std::atomic<bool> m_stopToken{false};
    EngineParameters m_params{};
    SearchLimits m_limits;
    TimeManager m_timeManager;
    SearchStatistics m_stats;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

} // namespace Boson

#endif // BOSON_SEARCH_CONTROLLER_HPP