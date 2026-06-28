#ifndef BOSON_CORRECTION_HISTORY_TABLE_HPP
#define BOSON_CORRECTION_HISTORY_TABLE_HPP

#include "board/Position.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace Boson {

class CorrectionHistoryTable {
public:
    void clear() noexcept {
        m_table[0].fill(0);
        m_table[1].fill(0);
    }

    // Direct O(1) table indexing using a 14-bit mask from the position's hash key
    [[nodiscard]] int getCorrection(Color sideToMove, uint64_t hashKey) const noexcept {
        size_t sideIdx = static_cast<size_t>(sideToMove);
        size_t tableIdx = static_cast<size_t>(hashKey & TABLE_MASK);
        return m_table[sideIdx][tableIdx];
    }

    // Update rule to accumulate evaluation bias over time
    void updateCorrection(Color sideToMove, uint64_t hashKey, int depth, int searchScore, int staticEvalScore) noexcept {
        // Discrepancy calculation between reality (search) and estimation (static eval)
        int discrepancy = searchScore - staticEvalScore;
        
        // Clamp out extreme tactical spikes/noise to focus purely on positional bias
        if (std::abs(discrepancy) > 1200) return; 

        size_t sideIdx = static_cast<size_t>(sideToMove);
        size_t tableIdx = static_cast<size_t>(hashKey & TABLE_MASK);

        // Exponential smoothing update rule: weight the adjustment higher based on search depth stability
        int weight = std::min(depth * depth, 256);
        int currentVal = m_table[sideIdx][tableIdx];
        
        // Update via standard history integration framework
        int newVal = currentVal + (discrepancy * weight - currentVal) / 1024;
        m_table[sideIdx][tableIdx] = std::clamp(newVal, -MAX_CORRECTION, MAX_CORRECTION);
    }

    void normalize() noexcept {
        for (auto& sideTable : m_table) {
            for (int& score : sideTable) {
                score /= 2;
            }
        }
    }

private:
    static constexpr size_t TABLE_SIZE = 16384; // 14-bit resolution table (16K entries)
    static constexpr uint64_t TABLE_MASK = TABLE_SIZE - 1;
    static constexpr int MAX_CORRECTION = 512; // Cap adjustments at ~5.12 pawns max

    // Memory layout: 2 sides * 16384 entries * 4 bytes = 128 KB (fits cleanly inside L2 cache)
    std::array<std::array<int, TABLE_SIZE>, 2> m_table{};
};

} // namespace Boson

#endif // BOSON_CORRECTION_HISTORY_TABLE_HPP