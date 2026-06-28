#ifndef BOSON_CONTINUATION_HISTORY_TABLE_HPP
#define BOSON_CONTINUATION_HISTORY_TABLE_HPP

#include "board/Move.hpp"
#include <array>
#include <algorithm>

namespace Boson {

class ContinuationHistoryTable {
public:
    void clear() noexcept {
        for (auto& pieceRow : m_table) {
            for (auto& prevSqRow : pieceRow) {
                prevSqRow.fill(0);
            }
        }
    }

    // Direct cache-friendly array insertion mapping: [Piece][PrevToSq][CurrentToSq]
    void updateScore(Piece p, Square prevTo, Square currTo, int bonus) noexcept {
        if (p != Piece::None && prevTo != Square::None && currTo != Square::None) {
            int& score = m_table[static_cast<size_t>(p)][static_cast<size_t>(prevTo)][static_cast<size_t>(currTo)];
            score += bonus;
            // Bound caps to prevent standard integer overflow before global normalization sweeps
            if (score > 32000) score = 32000;
            if (score < -32000) score = -32000;
        }
    }

    [[nodiscard]] int getScore(Piece p, Square prevTo, Square currTo) const noexcept {
        if (p == Piece::None || prevTo == Square::None || currTo == Square::None) return 0;
        return m_table[static_cast<size_t>(p)][static_cast<size_t>(prevTo)][static_cast<size_t>(currTo)];
    }

    // Unified Aging Hook hooked into Search's central normalization sweep
    void normalize() noexcept {
        for (auto& pieceRow : m_table) {
            for (auto& prevSqRow : pieceRow) {
                for (int& score : prevSqRow) {
                    score /= 2;
                }
            }
        }
    }

private:
    // Dense flat layout: 12 pieces * 64 previous squares * 64 current squares * 4 bytes = 196 KB
    std::array<std::array<std::array<int, 64>, 64>, 12> m_table{};
};

} // namespace Boson

#endif // BOSON_CONTINUATION_HISTORY_TABLE_HPP