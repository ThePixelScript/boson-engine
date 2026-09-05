#ifndef BOSON_CORRECTION_HISTORY_TABLE_HPP
#define BOSON_CORRECTION_HISTORY_TABLE_HPP

#include "board/Position.hpp"
#include "search/Zobrist.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <bit>

namespace Boson {

class CorrectionHistoryTable {
public:
    static constexpr size_t TABLE_SIZE = 16384; // 14-bit resolution table (16K entries per side)
    static constexpr uint64_t TABLE_MASK = TABLE_SIZE - 1;
    static constexpr int MAX_CORRECTION = 1024; // Cap adjustments at +/- 1024 cp

    // Stage 1: Initialize / Clear table at search boundaries
    void initialize() noexcept {
        clear();
    }

    void clear() noexcept {
        m_table[0].fill(0);
        m_table[1].fill(0);
    }

    // Feature key extraction: Pawn structure Zobrist hash
    [[nodiscard]] static uint64_t computePawnKey(const Position& pos) noexcept {
        uint64_t pawnKey = 0;
        Bitboard wp = pos.getPieceBitboard(Piece::WhitePawn);
        while (wp) {
            int sq = std::countr_zero(wp);
            pawnKey ^= Zobrist::s_pieces[static_cast<size_t>(Piece::WhitePawn)][sq];
            wp &= wp - 1;
        }
        Bitboard bp = pos.getPieceBitboard(Piece::BlackPawn);
        while (bp) {
            int sq = std::countr_zero(bp);
            pawnKey ^= Zobrist::s_pieces[static_cast<size_t>(Piece::BlackPawn)][sq];
            bp &= bp - 1;
        }
        return pawnKey;
    }

    [[nodiscard]] static size_t getBucketIndex(const Position& pos) noexcept {
        return static_cast<size_t>(computePawnKey(pos) & TABLE_MASK);
    }

    // Stage 2: Probe evaluation bias for the given position
    [[nodiscard]] int probe(const Position& pos) const noexcept {
        size_t sideIdx = static_cast<size_t>(pos.getSideToMove());
        size_t tableIdx = getBucketIndex(pos);
        return m_table[sideIdx][tableIdx];
    }

    // Direct O(1) table indexing using a 14-bit mask from hash key
    [[nodiscard]] int getCorrection(Color sideToMove, uint64_t hashKey) const noexcept {
        size_t sideIdx = static_cast<size_t>(sideToMove);
        size_t tableIdx = static_cast<size_t>(hashKey & TABLE_MASK);
        return m_table[sideIdx][tableIdx];
    }

    // Stage 3: Bounded Update with depth-scaled gravity decay
    void update(const Position& pos, int depth, int searchScore, int staticEvalScore) noexcept {
        int discrepancy = searchScore - staticEvalScore;
        // Clamp out extreme tactical spikes/noise to focus purely on positional bias
        if (std::abs(discrepancy) > 1200) return; 

        size_t sideIdx = static_cast<size_t>(pos.getSideToMove());
        size_t tableIdx = getBucketIndex(pos);

        int weight = std::min(depth * depth, 256);
        int currentVal = m_table[sideIdx][tableIdx];
        
        int newVal = currentVal + (discrepancy * weight - currentVal) / 1024;
        m_table[sideIdx][tableIdx] = std::clamp(newVal, -MAX_CORRECTION, MAX_CORRECTION);
    }

    void updateCorrection(Color sideToMove, uint64_t hashKey, int depth, int searchScore, int staticEvalScore) noexcept {
        int discrepancy = searchScore - staticEvalScore;
        if (std::abs(discrepancy) > 1200) return; 

        size_t sideIdx = static_cast<size_t>(sideToMove);
        size_t tableIdx = static_cast<size_t>(hashKey & TABLE_MASK);

        int weight = std::min(depth * depth, 256);
        int currentVal = m_table[sideIdx][tableIdx];
        
        int newVal = currentVal + (discrepancy * weight - currentVal) / 1024;
        m_table[sideIdx][tableIdx] = std::clamp(newVal, -MAX_CORRECTION, MAX_CORRECTION);
    }

    // Stage 4: Normalize / Age hook
    void normalize() noexcept {
        for (auto& sideTable : m_table) {
            for (int& score : sideTable) {
                score /= 2;
            }
        }
    }

    void age() noexcept {
        normalize();
    }

private:
    // Memory layout: 2 sides * 16384 entries * 4 bytes = 128 KB (fits cleanly inside L2 cache)
    std::array<std::array<int, TABLE_SIZE>, 2> m_table{};
};

} // namespace Boson

#endif // BOSON_CORRECTION_HISTORY_TABLE_HPP