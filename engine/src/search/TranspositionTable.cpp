#include "search/TranspositionTable.hpp"
#include <bit>
#include <algorithm>

namespace Boson {

TranspositionTable::TranspositionTable(size_t megaBytes) noexcept {
    size_t bytes = megaBytes * 1024 * 1024;
    size_t rawEntries = bytes / sizeof(TTEntry);

    // Enforce power-of-two table sizing for optimal masking operations
    m_entryCount = 1ULL << (63 - std::countl_zero(rawEntries));
    m_mask = m_entryCount - 1;
    m_table.resize(m_entryCount, TTEntry{0ULL, Move(), 0, 0, 0, 0});
}

void TranspositionTable::clear() noexcept {
    std::fill(m_table.begin(), m_table.end(), TTEntry{0ULL, Move(), 0, 0, 0, 0});
    m_probes = 0;
    m_hits = 0;
    m_collisions = 0;
    m_cutoffs = 0;
}

void TranspositionTable::store(uint64_t key, int score, Move bestMove, int depth, TTNodeType type, uint8_t generation) noexcept {
    size_t index = key & m_mask;
    TTEntry& entry = m_table[index];

    // Depth-preferred replacement scheme
    if (entry.key == 0ULL || depth >= entry.depth || entry.generation != generation || key == entry.key) {
        entry.key = key;
        entry.score = static_cast<int16_t>(score);
        entry.bestMove = bestMove;
        entry.depth = static_cast<int16_t>(depth);
        entry.flag = static_cast<uint8_t>(type);
        entry.generation = generation;
    }
}

bool TranspositionTable::probe(uint64_t key, int& score, Move& bestMove, int& depth, TTNodeType& type, int alpha, int beta) noexcept {
    m_probes++;
    size_t index = key & m_mask;
    const TTEntry& entry = m_table[index];

    if (entry.key == key) {
        m_hits++;
        bestMove = entry.bestMove;
        depth = entry.depth;
        type = static_cast<TTNodeType>(entry.flag);
        score = entry.score;

        // Ensure bounds constraints line up precisely with alpha-beta windows
        if (entry.depth >= depth) {
            if (type == TTNodeType::Exact) {
                m_cutoffs++;
                return true;
            }
            if (type == TTNodeType::LowerBound && score >= beta) {
                m_cutoffs++;
                return true;
            }
            if (type == TTNodeType::UpperBound && score <= alpha) {
                m_cutoffs++;
                return true;
            }
        }
    } else if (entry.key != 0ULL) {
        m_collisions++;
    }
    return false;
}

} // namespace Boson