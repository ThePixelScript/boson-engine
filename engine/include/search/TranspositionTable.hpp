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
    uint64_t key;
    int score;
    Move bestMove;
    int depth;
    TTNodeType type;
    uint8_t generation;
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t megaBytes) noexcept;
    void clear() noexcept;

    void store(uint64_t key, int score, Move bestMove, int depth, TTNodeType type, uint8_t generation) noexcept;
    bool probe(uint64_t key, int& score, Move& bestMove, int& depth, TTNodeType& type, int alpha, int beta) noexcept;

private:
    std::vector<TTEntry> m_table;
    size_t m_entryCount;
};

} // namespace Boson

#endif // BOSON_TRANSPOSITION_TABLE_HPP