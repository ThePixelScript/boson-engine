#ifndef BOSON_BOARD_PRINTER_HPP
#define BOSON_BOARD_PRINTER_HPP

#include "board/Position.hpp"

namespace Boson {

class BoardPrinter {
public:
    enum class Mode : uint8_t {
        Human,
        Debug
    };

    static void print(const Position& pos, Mode mode) noexcept;
};

} // namespace Boson

#endif // BOSON_BOARD_PRINTER_HPP