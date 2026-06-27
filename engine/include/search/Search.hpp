#ifndef BOSON_SEARCH_HPP
#define BOSON_SEARCH_HPP

#include "board/Position.hpp"
#include "board/MoveList.hpp"
#include "search/TranspositionTable.hpp"

namespace Boson {

class Search {
public:
    static uint64_t perft(Position& pos, int depth) noexcept;
    static void divide(Position& pos, int depth) noexcept;
    static int runSearch(Position& pos, int maxDepth) noexcept;

private:
    static int negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept;
    static int evaluate(const Position& pos) noexcept;

    static TranspositionTable s_tt;
    static uint64_t m_nodes;

    static constexpr int INF = 32000;
    static constexpr int MATE = 31000;
};

} // namespace Boson

#endif // BOSON_SEARCH_HPP