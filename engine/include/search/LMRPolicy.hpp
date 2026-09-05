#ifndef BOSON_LMR_POLICY_HPP
#define BOSON_LMR_POLICY_HPP

#include <algorithm>
#include <cmath>

namespace Boson {

class LMRPolicy {
public:
    static constexpr double BASE_REDUCTION = 0.5;
    static constexpr double LOG_DIVISOR = 1.95;
    static constexpr int MIN_DEPTH = 3;
    static constexpr int MIN_MOVE_COUNT = 4;

    static void initializeTable() noexcept {
        for (int depth = 0; depth < 64; ++depth) {
            for (int moveCount = 0; moveCount < 64; ++moveCount) {
                if (depth >= MIN_DEPTH && moveCount >= MIN_MOVE_COUNT) {
                    // Classic base logarithmic scaling policy: R(d, m) = BASE + ln(d)*ln(m) / DIVISOR
                    double reduction = BASE_REDUCTION + (std::log(depth) * std::log(moveCount) / LOG_DIVISOR);
                    s_reductionTable[depth][moveCount] = std::min(depth - 1, static_cast<int>(reduction));
                } else {
                    s_reductionTable[depth][moveCount] = 0;
                }
            }
        }
        s_initialized = true;
    }

    [[nodiscard]] static int getReduction(int depth, int moveCount) noexcept {
        // Guarantee initialized on first call if not already done
        [[unlikely]] if (!s_initialized) {
            initializeTable();
        }

        if (depth < 0 || moveCount < 0) return 0;
        if (depth >= 64) depth = 63;
        if (moveCount >= 64) moveCount = 63;
        return s_reductionTable[depth][moveCount];
    }

    [[nodiscard]] static bool isInitialized() noexcept { return s_initialized; }

private:
    static inline bool s_initialized = false;
    static inline int s_reductionTable[64][64] = {};
};

using LMR = LMRPolicy;

} // namespace Boson

#endif // BOSON_LMR_POLICY_HPP