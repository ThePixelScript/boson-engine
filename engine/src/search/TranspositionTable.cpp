#include "search/TranspositionTable.hpp"
#include <algorithm>

namespace Boson {

TranspositionTable::TranspositionTable(size_t megaBytes) noexcept {
    size_t bytes = megaBytes * 1024 * 1024;
    m_capacity = bytes / sizeof(TTEntry);
    if (m_capacity > 0) {
        m_table.resize(m_capacity);
    }
}

void TranspositionTable::clear() noexcept {
    std::fill(m_table.begin(), m_table.end(), TTEntry{});
    m_probes = 0;
    m_hits = 0;
    m_collisions = 0;
    m_cutoffs = 0;
}

void TranspositionTable::store(uint64_t key, int score, Move bestMove, int depth, TTNodeType type, uint8_t generation) noexcept {
    if (m_capacity == 0) return;
    size_t index = key % m_capacity;
    TTEntry& entry = m_table[index];

    // 🎯 Protect deeper Exact PV entries from being overwritten by shallow aspiration noise
    if (entry.key == key) {
        if (entry.depth > depth && entry.type == TTNodeType::Exact && type != TTNodeType::Exact) {
            return; 
        }
    }

    // Depth-preferred replacement scheme
    if (entry.key != key || depth >= entry.depth || type == TTNodeType::Exact) {
        entry.key = key;
        entry.score = static_cast<int16_t>(score);
        entry.bestMove = bestMove;
        entry.depth = static_cast<int8_t>(depth);
        entry.type = type;
        entry.generation = generation;
    }
}

bool TranspositionTable::probe(uint64_t key, int& score, Move& bestMove, int& depth, TTNodeType& type, int alpha, int beta) noexcept {
    m_probes++;
    if (m_capacity == 0) return false;

    size_t index = key % m_capacity;
    const TTEntry& entry = m_table[index];

    if (entry.key == key) {
        m_hits++;
        bestMove = entry.bestMove;
        depth = entry.depth;
        type = entry.type;
        score = entry.score;

        if (entry.type == TTNodeType::Exact) {
            m_cutoffs++;
            return true;
        }
        if (entry.type == TTNodeType::LowerBound && score >= beta) {
            m_cutoffs++;
            return true;
        }
        if (entry.type == TTNodeType::UpperBound && score <= alpha) {
            m_cutoffs++;
            return true;
        }
    } else if (entry.key != 0) {
        m_collisions++;
    }

    return false;
}

} // namespace Boson