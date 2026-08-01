#ifndef BOSON_LMR_POLICY_HPP
#define BOSON_LMR_POLICY_HPP

#include <algorithm>
#include <cmath>

namespace Boson {

class LMRPolicy {
public:
    static void initializeTable() noexcept {
        for (int depth = 0; depth < 64; ++depth) {
            for (int moveCount = 0; moveCount < 64; ++moveCount) {
                if (depth >= 3 && moveCount >= 4) {
                    // Classic base logarithmic scaling policy
                    double reduction = 0.5 + std::log(depth) * std::log(moveCount) / 1.95;
                    s_reductionTable[depth][moveCount] = std::min(depth - 1, static_cast<int>(reduction));
                } else {
                    s_reductionTable[depth][moveCount] = 0;
                }
            }
        }
    }

    [[nodiscard]] static int getReduction(int depth, int moveCount) noexcept {
        // Guarantee initialized on first call if not already done
        [[unlikely]] if (!s_initialized) {
            initializeTable();
            s_initialized = true;
        }

        if (depth >= 64) depth = 63;
        if (moveCount >= 64) moveCount = 63;
        return s_reductionTable[depth][moveCount];
    }

private:
    static inline bool s_initialized = false;
    static inline int s_reductionTable[64][64] = {};
};

} // namespace Boson

#endif // BOSON_LMR_POLICY_HPP