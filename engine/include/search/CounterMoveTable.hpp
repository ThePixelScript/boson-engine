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
        for (auto& row : m_pieceTable) {
            for (auto& cell : row) {
                cell = Move();
            }
        }
    }

    // Direct O(1) array lookup for hot path cache locality: [from][to]
    void store(Square prevFrom, Square prevTo, Move counterMove) noexcept {
        if (prevFrom != Square::None && prevTo != Square::None) {
            m_table[static_cast<size_t>(prevFrom)][static_cast<size_t>(prevTo)] = counterMove;
        }
    }

    [[nodiscard]] Move getCounterMove(Square prevFrom, Square prevTo) const noexcept {
        if (prevFrom == Square::None || prevTo == Square::None) return Move();
        return m_table[static_cast<size_t>(prevFrom)][static_cast<size_t>(prevTo)];
    }

    // Secondary O(1) piece-to table: [piece][toSquare] (e.g. std::array<std::array<Move, 64>, 12>)
    void store(Piece prevPiece, Square prevTo, Move counterMove) noexcept {
        if (prevPiece != Piece::None && prevTo != Square::None) {
            m_pieceTable[static_cast<size_t>(prevPiece)][static_cast<size_t>(prevTo)] = counterMove;
        }
    }

    [[nodiscard]] Move getCounterMove(Piece prevPiece, Square prevTo) const noexcept {
        if (prevPiece == Piece::None || prevTo == Square::None) return Move();
        return m_pieceTable[static_cast<size_t>(prevPiece)][static_cast<size_t>(prevTo)];
    }

private:
    // Primary index: 64 * 64 * sizeof(Move) = 8 KB (Fits cleanly inside L1 Data Cache)
    std::array<std::array<Move, 64>, 64> m_table{};
    // Secondary index: 12 * 64 * sizeof(Move) = 1.5 KB
    std::array<std::array<Move, 64>, 12> m_pieceTable{};
};

} // namespace Boson

#endif // BOSON_COUNTER_MOVE_TABLE_HPP