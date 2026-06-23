#ifndef BOSON_MOVE_HPP
#define BOSON_MOVE_HPP

#include <cstdint>
#include "Square.hpp"
#include "Piece.hpp"

namespace Boson {

class Move {
public:
enum class MoveType : uint8_t {
Normal,
Promotion,
EnPassant,
Castling
};

constexpr Move() noexcept : m_data(0) {}

constexpr Move(Square from, Square to, MoveType type = MoveType::Normal, PieceType promo = PieceType::None) noexcept {
m_data = static_cast<uint16_t>(from) |
(static_cast<uint16_t>(to) << 6) |
(static_cast<uint16_t>(type) << 12) |
(static_cast<uint16_t>(promo) << 14);
}

constexpr Square getFromSquare() const noexcept { return static_cast<Square>(m_data & 0x3F); }
constexpr Square getToSquare() const noexcept { return static_cast<Square>((m_data >> 6) & 0x3F); }
constexpr MoveType getMoveType() const noexcept { return static_cast<MoveType>((m_data >> 12) & 0x3); }
constexpr PieceType getPromotionType() const noexcept { return static_cast<PieceType>((m_data >> 14) & 0x3); }
constexpr uint16_t getRawData() const noexcept { return m_data; }

private:
uint16_t m_data; // Bit packed layout: [0-5] From, [6-11] To, [12-13] Type, [14-15] Promotion
};

} // namespace Boson

#endif // BOSON_MOVE_HPP