#ifndef BOSON_MOVE_LIST_HPP
#define BOSON_MOVE_LIST_HPP

#include <array>
#include <cstddef>
#include "Move.hpp"

namespace Boson {

class MoveList {
public:
    constexpr MoveList() noexcept : m_count(0) {}

    constexpr void push_back(const Move& move) noexcept {
        if (m_count < m_storage.size()) {
            m_storage[m_count++] = move;
        }
    }

    constexpr const Move& operator[](size_t index) const noexcept { return m_storage[index]; }
    constexpr size_t size() const noexcept { return m_count; }
    constexpr void clear() noexcept { m_count = 0; }

private:
    std::array<Move, 256> m_storage; // 256 moves handles any valid chess configuration easily
    size_t m_count;
};

} // namespace Boson

#endif // BOSON_MOVE_LIST_HPP