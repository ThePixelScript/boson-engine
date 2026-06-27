#ifndef BOSON_TIME_MANAGER_HPP
#define BOSON_TIME_MANAGER_HPP

#include "SearchLimits.hpp"
#include "board/Position.hpp"

namespace Boson {

class TimeManager {
public:
    void calculateLimits(const SearchLimits& limits, Color sideToMove) noexcept {
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
            // Allocate 1/20th of remaining base time pool plus half of increment safety buffer
            m_softLimitMs = (timeAvailable / 20) + (increment / 2);
            // Panic hard cutoff limits search boundary to 1/4th of remaining entire clock pool
            m_hardLimitMs = timeAvailable / 4;
            
            // Guarantee safe bounds clamp
            if (m_softLimitMs > timeAvailable) m_softLimitMs = timeAvailable - 50;
            if (m_softLimitMs < 1) m_softLimitMs = 1;
            if (m_hardLimitMs > timeAvailable) m_hardLimitMs = timeAvailable - 20;
            
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