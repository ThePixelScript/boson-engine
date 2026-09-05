#ifndef BOSON_PVLINE_HPP
#define BOSON_PVLINE_HPP

#include <cstddef>
#include <array>
#include "board/Move.hpp"

namespace Boson {

struct PVLine {
    size_t count = 0;
    std::array<Move, 64> moves{};
};

} // namespace Boson

#endif // BOSON_PVLINE_HPP
