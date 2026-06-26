#include "search/TranspositionTable.hpp"

namespace Boson {

TranspositionTable::TranspositionTable(size_t megaBytes) noexcept {
    size_t bytes = megaBytes * 1024 * 1024;
    m_entryCount = bytes / sizeof(TTEntry);
    m_table.resize(m_entryCount, TTEntry{0ULL, 0, Move(), 0, TTNodeType::Exact, 0});
}

void TranspositionTable::clear() noexcept {
    std::fill(m_table.begin(), m_table.end(), TTEntry{0ULL, 0, Move(), 0, TTNodeType::Exact, 0});
}

void TranspositionTable::store(uint64_t key, int score, Move bestMove, int depth, TTNodeType type, uint8_t generation) noexcept {
    size_t index = key % m_entryCount;
    TTEntry& entry = m_table[index];

    // Depth-preferred replacement logic layout
    if (entry.key == 0ULL || entry.generation != generation || depth >= entry.depth) {
        entry.key = key;
        entry.score = score;
        entry.bestMove = bestMove;
        entry.depth = depth;
        entry.type = type;
        entry.generation = generation;
    }
}

bool TranspositionTable::probe(uint64_t key, int& score, Move& bestMove, int& depth, TTNodeType& type, int alpha, int beta) noexcept {
    size_t index = key % m_entryCount;
    const TTEntry& entry = m_table[index];

    if (entry.key == key) {
        bestMove = entry.bestMove;
        depth = entry.depth;
        type = entry.type;

        // Verify bounds constraint validation layout
        if (entry.type == TTNodeType::Exact) {
            score = entry.score;
            return true;
        }
        if (entry.type == TTNodeType::UpperBound && entry.score <= alpha) {
            score = alpha;
            return true;
        }
        if (entry.type == TTNodeType::LowerBound && entry.score >= beta) {
            score = beta;
            return true;
        }
    }
    return false;
}

} // namespace Boson