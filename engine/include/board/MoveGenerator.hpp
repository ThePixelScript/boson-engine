#ifndef BOSON_MOVE_GENERATOR_HPP
#define BOSON_MOVE_GENERATOR_HPP

#include <array>
#include "Position.hpp"
#include "MoveList.hpp"

namespace Boson {

class MoveGenerator {
public:
    static void initializeTables() noexcept;

    // Read-Only Generation Subsystems
    static void generateKnightMoves(const Position& pos, MoveList& moves) noexcept;
    static void generateKingMoves(const Position& pos, MoveList& moves) noexcept;
    static void generatePawnMoves(const Position& pos, MoveList& moves) noexcept;

private:
    static std::array<Bitboard, 64> s_knightAttacks;
    static std::array<Bitboard, 64> s_kingAttacks;
    static bool s_initialized;

    // Pawn Generation Internal Components
    static void generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept;
    static void generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept;
};

} // namespace Boson

#endif // BOSON_MOVE_GENERATOR_HPP