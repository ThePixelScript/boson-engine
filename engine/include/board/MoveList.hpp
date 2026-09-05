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

    // Mutable indexing operator for in-place reordering (Phase AA requirement)
    constexpr Move& operator[](size_t index) noexcept { return m_storage[index]; }
    
    // Read-only indexing operator
    constexpr const Move& operator[](size_t index) const noexcept { return m_storage[index]; }
    
    constexpr size_t size() const noexcept { return m_count; }
    constexpr bool empty() const noexcept { return m_count == 0; }
    constexpr size_t capacity() const noexcept { return m_storage.size(); }
    constexpr void clear() noexcept { m_count = 0; }

    constexpr Move* data() noexcept { return m_storage.data(); }
    constexpr const Move* data() const noexcept { return m_storage.data(); }

    constexpr auto begin() noexcept { return m_storage.begin(); }
    constexpr auto end() noexcept { return m_storage.begin() + m_count; }
    constexpr auto begin() const noexcept { return m_storage.cbegin(); }
    constexpr auto end() const noexcept { return m_storage.cbegin() + m_count; }
    constexpr auto cbegin() const noexcept { return m_storage.cbegin(); }
    constexpr auto cend() const noexcept { return m_storage.cbegin() + m_count; }

private:
    std::array<Move, 256> m_storage;
    size_t m_count;
};

} // namespace Boson

#endif // BOSON_MOVE_LIST_HPP