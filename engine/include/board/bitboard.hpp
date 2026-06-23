#ifndef BOSON_BITBOARD_HPP
#define BOSON_BITBOARD_HPP

#include <cstdint>
#include "Square.hpp"

namespace Boson {

using Bitboard = uint64_t;

namespace Bitboards {
constexpr Bitboard Empty = 0ULL;

constexpr Bitboard getSquareBit(Square sq) noexcept {
if (sq == Square::None) return Empty;
return 1ULL << static_cast<uint8_t>(sq);
}
}

} // namespace Boson

#endif // BOSON_BITBOARD_HPP