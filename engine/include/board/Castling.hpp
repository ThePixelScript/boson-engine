#ifndef BOSON_CASTLING_HPP
#define BOSON_CASTLING_HPP

#include <cstdint>

namespace Boson {

enum class CastlingRights : uint8_t {
None = 0,
WhiteOO = 1 << 0,
WhiteOOO = 1 << 1,
BlackOO = 1 << 2,
BlackOOO = 1 << 3,
WhiteSide = WhiteOO | WhiteOOO,
BlackSide = BlackOO | BlackOOO,
All = WhiteSide | BlackSide
};

constexpr CastlingRights operator|(CastlingRights lhs, CastlingRights rhs) noexcept {
return static_cast<CastlingRights>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr CastlingRights operator&(CastlingRights lhs, CastlingRights rhs) noexcept {
return static_cast<CastlingRights>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

} // namespace Boson

#endif // BOSON_CASTLING_HPP