#ifndef BOSON_MOVE_GENERATOR_HPP
#define BOSON_MOVE_GENERATOR_HPP

#include <array>
#include "Position.hpp"
#include "MoveList.hpp"

namespace Boson {

class MoveGenerator {
public:
    // Initializes lookup tables at engine boot time
    static void initializeTables() noexcept;

    // Phase A: Generates all pseudo-legal Knight moves from the active Position
    static void generateKnightMoves(const Position& pos, MoveList& moves) noexcept;

private:
    static std::array<Bitboard, 64> s_knightAttacks;
    static bool s_initialized;
};

} // namespace Boson

#endif // BOSON_MOVE_GENERATOR_HPP