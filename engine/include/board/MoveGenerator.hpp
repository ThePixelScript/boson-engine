#ifndef BOSON_MOVE_GENERATOR_HPP
#define BOSON_MOVE_GENERATOR_HPP

#include <array>
#include "Position.hpp"
#include "MoveList.hpp"

namespace Boson {

class MoveGenerator {
public:
    static void initializeTables() noexcept;

    // Core Read-Only Generation Entry Points
    static void generateKnightMoves(const Position& pos, MoveList& moves) noexcept;
    static void generateKingMoves(const Position& pos, MoveList& moves) noexcept;
    static void generatePawnMoves(const Position& pos, MoveList& moves) noexcept;
    static void generateSlidingMoves(const Position& pos, MoveList& moves) noexcept;

    // Public Attack Interface (Exposed for future check-detection and Magic Bitboard drop-ins)
    static Bitboard getRookAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getBishopAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getQueenAttacks(Square sq, Bitboard occupancy) noexcept {
        return getRookAttacks(sq, occupancy) | getBishopAttacks(sq, occupancy);
    }

private:
    static std::array<Bitboard, 64> s_knightAttacks;
    static std::array<Bitboard, 64> s_kingAttacks;
    
    // Sliding Ray Tables: 64 squares, 4 directions per sliding type
    // Rooks: 0=N, 1=S, 2=E, 3=W | Bishops: 0=NE, 1=NW, 2=SE, 3=SW
    static std::array<std::array<Bitboard, 4>, 64> s_rookRays;
    static std::array<std::array<Bitboard, 4>, 64> s_bishopRays;
    static bool s_initialized;

    // Sliding Piece Internal Traversal Engine
    static Bitboard calculateSlidingAttacks(Square sq, Bitboard occupancy, const std::array<Bitboard, 4>& rays, const std::array<int, 4>& shifts) noexcept;
    
    static void generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept;
    static void generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept;
};

} // namespace Boson

#endif // BOSON_MOVE_GENERATOR_HPP