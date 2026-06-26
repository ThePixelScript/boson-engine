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

    // Public Attack Interface
    static Bitboard getRookAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getBishopAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getQueenAttacks(Square sq, Bitboard occupancy) noexcept {
        return getRookAttacks(sq, occupancy) | getBishopAttacks(sq, occupancy);
    }

    static bool isSquareAttacked(const Position& pos, Square sq, Color attacker) noexcept;
    static bool inCheck(const Position& pos, Color side) noexcept;
    static void generateLegalMoves(Position& pos, MoveList& legalMoves) noexcept;

private:
    static std::array<Bitboard, 64> s_knightAttacks;
    static std::array<Bitboard, 64> s_kingAttacks;
    
    // Sliding Ray Tables
    static std::array<std::array<Bitboard, 4>, 64> s_rookRays;
    static std::array<std::array<Bitboard, 4>, 64> s_bishopRays;
    static bool s_initialized;

    // Sliding Piece Internal Traversal Engine - Explicit 5-Argument Signature
    static Bitboard calculateSlidingAttacks(Square sq, Bitboard occupancy, const std::array<Bitboard, 4>& rays, const std::array<int, 4>& shifts, bool isRook) noexcept;
    
    static void generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept;
    static void generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept;
};

} // namespace Boson

#endif // BOSON_MOVE_GENERATOR_HPP