#ifndef BOSON_CONTINUATION_HISTORY_TABLE_HPP
#define BOSON_CONTINUATION_HISTORY_TABLE_HPP

#include "board/Move.hpp"
#include <array>
#include <algorithm>

namespace Boson {

class ContinuationHistoryTable {
public:
    static constexpr int MAX_HISTORY = 16384;
    static constexpr int MIN_HISTORY = -16384;

    // Stage 1: Initialize / Clear table at search boundaries
    void initialize() noexcept {
        clear();
    }

    void clear() noexcept {
        for (auto& pieceRow : m_table) {
            for (auto& prevSqRow : pieceRow) {
                prevSqRow.fill(0);
            }
        }
    }

    // Stage 2: Probe contextual score for piece and move history
    [[nodiscard]] int probe(Piece p, Square prevTo, Square currTo) const noexcept {
        return getScore(p, prevTo, currTo);
    }

    [[nodiscard]] int getScore(Piece p, Square prevTo, Square currTo) const noexcept {
        if (p == Piece::None || prevTo == Square::None || currTo == Square::None) return 0;
        return m_table[static_cast<size_t>(p)][static_cast<size_t>(prevTo)][static_cast<size_t>(currTo)];
    }

    // Stage 3: Bounded Update with Stockfish gravity decay clamped at +/- 16384
    void update(Piece p, Square prevTo, Square currTo, int bonus) noexcept {
        if (p != Piece::None && prevTo != Square::None && currTo != Square::None) {
            int& score = m_table[static_cast<size_t>(p)][static_cast<size_t>(prevTo)][static_cast<size_t>(currTo)];
            int clampedBonus = std::clamp(bonus, MIN_HISTORY, MAX_HISTORY);
            score += clampedBonus - (score * std::abs(clampedBonus) / MAX_HISTORY);
            score = std::clamp(score, MIN_HISTORY, MAX_HISTORY);
        }
    }

    void updateScore(Piece p, Square prevTo, Square currTo, int bonus) noexcept {
        update(p, prevTo, currTo, bonus);
    }

    // Stage 4: Aging / Normalize hook to decay scores across iterations
    void normalize() noexcept {
        for (auto& pieceRow : m_table) {
            for (auto& prevSqRow : pieceRow) {
                for (int& score : prevSqRow) {
                    score /= 2;
                }
            }
        }
    }

    void age() noexcept {
        normalize();
    }

private:
    // Dense flat layout: 12 pieces * 64 previous squares * 64 current squares * 4 bytes = 196 KB
    std::array<std::array<std::array<int, 64>, 64>, 12> m_table{};
};

} // namespace Boson

#endif // BOSON_CONTINUATION_HISTORY_TABLE_HPP