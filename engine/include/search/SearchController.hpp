#ifndef BOSON_SEARCH_CONTROLLER_HPP
#define BOSON_SEARCH_CONTROLLER_HPP

#include <atomic>
#include <chrono>
#include "SearchLimits.hpp"
#include "SearchStatistics.hpp"
#include "TimeManager.hpp"
#include "board/Position.hpp"

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
        m_timeManager.calculateLimits(limits, pos.getSideToMove());
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

private:
    SearchController() = default;

    std::atomic<bool> m_stopToken{false};
    SearchLimits m_limits;
    TimeManager m_timeManager;
    SearchStatistics m_stats;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

} // namespace Boson

#endif // BOSON_SEARCH_CONTROLLER_HPP