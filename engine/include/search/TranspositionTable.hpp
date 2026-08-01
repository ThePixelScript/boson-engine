#ifndef BOSON_TRANSPOSITION_TABLE_HPP
#define BOSON_TRANSPOSITION_TABLE_HPP

#include <vector>
#include <cstdint>
#include "board/Move.hpp"

namespace Boson {

enum class TTNodeType : uint8_t {
    Exact = 0,
    LowerBound = 1, // Fail-high (Beta cutoff)
    UpperBound = 2  // Fail-low (Alpha drop)
};

struct TTEntry {
    uint64_t key = 0;
    Move bestMove;
    int16_t score = 0;
    int8_t depth = 0;
    TTNodeType type = TTNodeType::Exact;
    uint8_t generation = 0;
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t megaBytes) noexcept;
    void clear() noexcept;

    void store(uint64_t key, int score, Move bestMove, int depth, TTNodeType type, uint8_t generation) noexcept;
    bool probe(uint64_t key, int& score, Move& bestMove, int& depth, TTNodeType& type, int alpha, int beta) noexcept;

    // Instrumentation Metrics
    uint64_t getProbes() const noexcept { return m_probes; }
    uint64_t getHits() const noexcept { return m_hits; }
    uint64_t getCollisions() const noexcept { return m_collisions; }
    uint64_t getCutoffs() const noexcept { return m_cutoffs; }

private:
    std::vector<TTEntry> m_table;
    size_t m_capacity = 0;

    // Diagnostic Counters
    uint64_t m_probes = 0;
    uint64_t m_hits = 0;
    uint64_t m_collisions = 0;
    uint64_t m_cutoffs = 0;
};

} // namespace Boson

#endif // BOSON_TRANSPOSITION_TABLE_HPP