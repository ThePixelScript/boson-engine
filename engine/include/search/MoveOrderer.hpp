#ifndef BOSON_MOVE_ORDERER_HPP
#define BOSON_MOVE_ORDERER_HPP

#include "board/MoveList.hpp"
#include "board/Position.hpp"
#include <array>

namespace Boson {

class MoveOrderer {
public:
    static constexpr int SCORE_TT          = 100000;
    static constexpr int SCORE_CAPTURES    = 50000;
    static constexpr int SCORE_PROMOTIONS  = 40000;
    static constexpr int SCORE_KILLER_1    = 30000;
    static constexpr int SCORE_KILLER_2    = 29000;
    static constexpr int SCORE_QUIET       = 0;

    static const std::array<std::array<int, 6>, 6> MVV_LVA;

    static void scoreAndSortMoves(
        const Position& pos, 
        MoveList& moves, 
        Move ttMove, 
        const std::array<std::array<Move, 2>, 64>& killerMoves,
        const std::array<std::array<uint32_t, 64>, 12>& historyTable,
        int ply,
        Move prevMove // Consumed context parameter
    ) noexcept;

private:
    static int getPieceIndex(Piece p) noexcept;
    static Piece findPieceAtSquare(const Position& pos, Square sq) noexcept;
};

} // namespace Boson

#endif // BOSON_MOVE_ORDERER_HPP