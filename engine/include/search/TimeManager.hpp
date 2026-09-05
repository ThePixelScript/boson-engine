#ifndef BOSON_TIME_MANAGER_HPP
#define BOSON_TIME_MANAGER_HPP

#include "SearchLimits.hpp"
#include "board/Position.hpp"
#include "config/EngineParameters.hpp"

namespace Boson {

class TimeManager {
public:
    void calculateLimits(const SearchLimits& limits, Color sideToMove, const TimeParameters& params = TimeParameters{}) noexcept {
        m_hasTimeLimit = false;
        m_softLimitMs = -1;
        m_hardLimitMs = -1;

        if (limits.movetime != -1) {
            m_softLimitMs = limits.movetime;
            m_hardLimitMs = limits.movetime;
            m_hasTimeLimit = true;
            return;
        }

        int64_t timeAvailable = (sideToMove == Color::White) ? limits.wtime : limits.btime;
        int64_t increment = (sideToMove == Color::White) ? limits.winc : limits.binc;

        if (timeAvailable != -1) {
            int allocDiv = (params.allocDivisor > 0) ? params.allocDivisor : 20;
            int incDiv = (params.incDivisor > 0) ? params.incDivisor : 2;
            m_softLimitMs = (timeAvailable / allocDiv) + (increment / incDiv);

            double mult = (params.hardLimitMultiplier > 0.0) ? params.hardLimitMultiplier : 3.0;
            m_hardLimitMs = static_cast<int64_t>(m_softLimitMs * mult);
            if (m_hardLimitMs > timeAvailable / 4) {
                m_hardLimitMs = timeAvailable / 4;
            }
            
            // Guarantee safe bounds clamp
            if (m_softLimitMs > timeAvailable) m_softLimitMs = timeAvailable - 50;
            if (m_softLimitMs < 1) m_softLimitMs = 1;
            if (m_hardLimitMs > timeAvailable) m_hardLimitMs = timeAvailable - 20;
            if (m_hardLimitMs < m_softLimitMs) m_hardLimitMs = m_softLimitMs;
            
            m_hasTimeLimit = true;
        }
    }

    [[nodiscard]] bool hasTimeLimit() const noexcept { return m_hasTimeLimit; }
    [[nodiscard]] int64_t getSoftLimit() const noexcept { return m_softLimitMs; }
    [[nodiscard]] int64_t getHardLimit() const noexcept { return m_hardLimitMs; }

private:
    bool m_hasTimeLimit = false;
    int64_t m_softLimitMs = -1;
    int64_t m_hardLimitMs = -1;
};

} // namespace Boson

#endif // BOSON_TIME_MANAGER_HPP