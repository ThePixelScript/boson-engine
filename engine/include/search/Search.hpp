#ifndef BOSON_SEARCH_HPP
#define BOSON_SEARCH_HPP

#include "board/Position.hpp"
#include "board/MoveList.hpp"

namespace Boson {

struct SearchStack {
    int ply;
};

class Search {
public:
    // Phase R: Tree Validation Framework
    static uint64_t perft(Position& pos, int depth) noexcept;
    static void divide(Position& pos, int depth) noexcept;

    // Phase T-V: Negamax Alpha-Beta Search Architecture
    static int runSearch(Position& pos, int maxDepth) noexcept;

private:
    static int negamax(Position& pos, int depth, int alpha, int beta, int ply) noexcept;
    static int evaluate(const Position& pos) noexcept;

    static constexpr int INF = 30000;
    static constexpr int MATE = 29000;
};

} // namespace Boson

#endif // BOSON_SEARCH_HPP