#ifndef BOSON_MOVE_HPP
#define BOSON_MOVE_HPP

#include <cstdint>
#include "Square.hpp"

namespace Boson {

class Move {
public:
    enum class Flags : uint8_t {
        None = 0,
        DoublePawnPush = 1,
        Castling = 2,
        EnPassant = 3,
        Promotion = 4
    };

    enum class PromotionPiece : uint8_t {
        None = 0, Knight = 1, Bishop = 2, Rook = 3, Queen = 4
    };

    // Default constructor (Creates a null move)
    constexpr Move() noexcept : m_data(0) {}

    // Packed Constructor: Changed cast to uint32_t to fully accommodate 18 bits safely
    constexpr Move(Square from, Square to, Flags flags = Flags::None, PromotionPiece promo = PromotionPiece::None) noexcept 
        : m_data(static_cast<uint32_t>(from) | 
                (static_cast<uint32_t>(to) << 6) | 
                (static_cast<uint32_t>(flags) << 12) | 
                (static_cast<uint32_t>(promo) << 15)) {}

    // Target Extractors
    constexpr Square getFromSquare() const noexcept { return static_cast<Square>(m_data & 0x3F); }
    constexpr Square getToSquare() const noexcept { return static_cast<Square>((m_data >> 6) & 0x3F); }
    constexpr Flags getFlags() const noexcept { return static_cast<Flags>((m_data >> 12) & 0x7); }
    constexpr PromotionPiece getPromotionPiece() const noexcept { return static_cast<PromotionPiece>((m_data >> 15) & 0x7); }

    // Evaluator Primitives
    constexpr bool isDoublePawnPush() const noexcept { return getFlags() == Flags::DoublePawnPush; }
    constexpr bool isCastling() const noexcept { return getFlags() == Flags::Castling; }
    constexpr bool isEnPassant() const noexcept { return getFlags() == Flags::EnPassant; }
    constexpr bool isPromotion() const noexcept { return getFlags() == Flags::Promotion; }

    constexpr uint32_t getRawData() const noexcept { return m_data; }

private:
    uint32_t m_data; // Changed from uint16_t to uint32_t to stop bit truncation
};

} // namespace Boson

#endif // BOSON_MOVE_HPP