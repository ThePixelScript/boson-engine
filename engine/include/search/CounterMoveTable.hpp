#ifndef BOSON_COUNTER_MOVE_TABLE_HPP
#define BOSON_COUNTER_MOVE_TABLE_HPP

#include "board/Move.hpp"
#include <array>

namespace Boson {

class CounterMoveTable {
public:
    void clear() noexcept {
        for (auto& row : m_table) {
            for (auto& cell : row) {
                cell = Move(); // Zero-initialized packed move data
            }
        }
    }

    // Direct O(1) array lookup for hot path cache locality
    void store(Square prevFrom, Square prevTo, Move counterMove) noexcept {
        if (prevFrom != Square::None && prevTo != Square::None) {
            m_table[static_cast<size_t>(prevFrom)][static_cast<size_t>(prevTo)] = counterMove;
        }
    }

    [[nodiscard]] Move getCounterMove(Square prevFrom, Square prevTo) const noexcept {
        if (prevFrom == Square::None || prevTo == Square::None) return Move();
        return m_table[static_cast<size_t>(prevFrom)][static_cast<size_t>(prevTo)];
    }

private:
    // Memory footprint: 64 * 64 * 4 bytes = 16 KB (Fits cleanly inside L1 Data Cache)
    std::array<std::array<Move, 64>, 64> m_table{};
};

} // namespace Boson

#endif // BOSON_COUNTER_MOVE_TABLE_HPP