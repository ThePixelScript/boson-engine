#ifndef BOSON_ZOBRIST_HPP
#define BOSON_ZOBRIST_HPP

#include <array>
#include <cstdint>
#include "board/Piece.hpp"
#include "board/Square.hpp"

namespace Boson {

class Zobrist {
public:
    static void initialize() noexcept;

    static uint64_t s_pieces[12][64];
    static uint64_t s_sideToMove;
    static uint64_t s_castling[16];
    static uint64_t s_enPassant[8]; // Hashed by file (A-H)

private:
    static uint64_t pseudoRandom64() noexcept;
    static bool s_initialized;
};

} // namespace Boson

#endif // BOSON_ZOBRIST_HPP