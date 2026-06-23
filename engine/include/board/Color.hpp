#ifndef BOSON_COLOR_HPP
#define BOSON_COLOR_HPP

#include <cstdint>

namespace Boson {

enum class Color : uint8_t {
White = 0,
Black = 1,
None = 2
};

constexpr Color operator!(Color color) noexcept {
return (color == Color::White) ? Color::Black : Color::White;
}

} // namespace Boson

#endif // BOSON_COLOR_HPP