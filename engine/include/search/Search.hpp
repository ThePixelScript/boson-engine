#ifndef BOSON_SEARCH_HPP
#define BOSON_SEARCH_HPP

#include "board/Position.hpp"
#include "board/MoveList.hpp"
#include "search/TranspositionTable.hpp"
#include "search/CounterMoveTable.hpp"
#include <array>

namespace Boson {

struct PVLine {
    size_t count = 0;
    std::array<Move, 64> moves{};
};

class Search {
public:
    static uint64_t perft(Position& pos, int depth) noexcept;
    static void divide(Position& pos, int depth) noexcept;
    static int runSearch(Position& pos, int maxDepth) noexcept;

    [[nodiscard]] static const CounterMoveTable& getCMH() noexcept { return s_cmTable; }

private:
    static int negamax(Position& pos, int depth, int alpha, int beta, int ply, PVLine& pv, bool allowNull, Move prevMove) noexcept;
    static int quiescence(Position& pos, int alpha, int beta, int ply) noexcept;
    static int evaluate(const Position& pos) noexcept;

    static TranspositionTable s_tt;
    static CounterMoveTable s_cmTable;
    static std::array<std::array<Move, 2>, 64> s_killerMoves;
    static std::array<std::array<uint32_t, 64>, 12> s_historyTable;

    static constexpr int INF = 32000;
    static constexpr int MATE = 31000;
    static constexpr uint64_t NODE_CHECK_PERIOD = 2048;
};

} // namespace Boson

#endif // BOSON_SEARCH_HPP