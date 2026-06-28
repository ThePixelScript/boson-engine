#pragma once

#include "board/Position.hpp"
#include "board/Move.hpp"
#include "board/MoveList.hpp"
#include <vector>
#include <array>

namespace Boson {

// Simple, fast collection container for generated paths
// using MoveList = std::vector<Move>;

class MoveGenerator {
public:
    // Global precalculated lookups infrastructure
    static void initializeTables() noexcept;
    
    // Non-Sliding attack vectors
    static Bitboard getKnightAttacks(Square sq) noexcept { return s_knightAttacks[static_cast<size_t>(sq)]; }
    static Bitboard getKingAttacks(Square sq) noexcept { return s_kingAttacks[static_cast<size_t>(sq)]; }

    // Sliding attack vectors
    static Bitboard getRookAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getBishopAttacks(Square sq, Bitboard occupancy) noexcept;
    static Bitboard getQueenAttacks(Square sq, Bitboard occupancy) noexcept;

    // Environmental state verification
    static bool isSquareAttacked(const Position& pos, Square sq, Color attacker) noexcept;
    static bool inCheck(const Position& pos, Color side) noexcept;

    // Core movement generation loops
    static void generateLegalMoves(Position& pos, MoveList& legalMoves) noexcept;
    static void generateTacticalMoves(Position& pos, MoveList& moves) noexcept;

    static void generateKnightMoves(const Position& pos, MoveList& moves) noexcept;
    static void generateKingMoves(const Position& pos, MoveList& moves) noexcept;
    static void generatePawnMoves(const Position& pos, MoveList& moves) noexcept;
    static void generateSlidingMoves(const Position& pos, MoveList& moves) noexcept;

private:
    // Sliding trajectory loop execution helper
    static Bitboard calculateSlidingAttacks(Square sq, Bitboard occupancy, const std::array<Bitboard, 4>& rays, const std::array<int, 4>& shifts, bool isRook) noexcept;

    // Pawn path emission sub-loops
    static void generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept;
    static void generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept;

    // Precalculated mask lookup memory arrays
    static std::array<Bitboard, 64> s_knightAttacks;
    static std::array<Bitboard, 64> s_kingAttacks;
    static std::array<std::array<Bitboard, 4>, 64> s_rookRays;
    static std::array<std::array<Bitboard, 4>, 64> s_bishopRays;
    static bool s_initialized;
};

} // namespace Boson