#ifndef BOSON_SEARCH_HPP
#define BOSON_SEARCH_HPP

#include "board/Position.hpp"
#include "board/MoveList.hpp"
#include "search/TranspositionTable.hpp"
#include <array>

namespace Boson {

class Search {
public:
    static uint64_t perft(Position& pos, int depth) noexcept;
    static void divide(Position& pos, int depth) noexcept;
    static int runSearch(Position& pos, int maxDepth) noexcept;

private:
    static int negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept;
    static int quiescence(Position& pos, int alpha, int beta, int ply) noexcept; // Phase AB
    static int evaluate(const Position& pos) noexcept;

    static TranspositionTable s_tt;
    static uint64_t m_nodes;
    static uint64_t m_qNodes; // Instrumentation metric for tactical leaf checks

    static std::array<std::array<Move, 2>, 64> s_killerMoves;
    static std::array<std::array<uint32_t, 64>, 12> s_historyTable;

    static constexpr int INF = 32000;
    static constexpr int MATE = 31000;
};

} // namespace Boson

#endif // BOSON_SEARCH_HPP