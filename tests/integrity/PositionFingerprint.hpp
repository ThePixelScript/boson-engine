#ifndef BOSON_POSITION_FINGERPRINT_HPP
#define BOSON_POSITION_FINGERPRINT_HPP

#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include "board/Position.hpp"
#include "board/Bitboard.hpp"
#include "board/Color.hpp"
#include "board/Piece.hpp"
#include "board/Castling.hpp"
#include "board/Square.hpp"

namespace Boson {

/// \brief Complete, bit-exact snapshot of all persistent and volatile state inside a Position.
struct PositionFingerprint {
    std::array<Bitboard, 12> pieces{};
    Bitboard whiteOccupancy{0};
    Bitboard blackOccupancy{0};
    Bitboard totalOccupancy{0};
    Color sideToMove{Color::White};
    CastlingRights castlingRights{CastlingRights::None};
    Square enPassantSquare{Square::None};
    uint16_t halfmoveClock{0};
    uint16_t fullmoveNumber{1};
    Square whiteKingSquare{Square::None};
    Square blackKingSquare{Square::None};
    uint64_t hashKey{0};

    /// \brief Capture an immutable fingerprint of the target board position.
    static PositionFingerprint capture(const Position& pos) noexcept {
        PositionFingerprint fp;
        for (size_t p = 0; p < 12; ++p) {
            fp.pieces[p] = pos.getPieceBitboard(static_cast<Piece>(p));
        }
        fp.whiteOccupancy = pos.getColorOccupancy(Color::White);
        fp.blackOccupancy = pos.getColorOccupancy(Color::Black);
        fp.totalOccupancy = pos.getTotalOccupancy();
        fp.sideToMove = pos.getSideToMove();
        fp.castlingRights = pos.getCastlingRights();
        fp.enPassantSquare = pos.getEnPassantSquare();
        fp.halfmoveClock = pos.getHalfmoveClock();
        fp.fullmoveNumber = pos.getFullmoveNumber();
        fp.whiteKingSquare = pos.getKingSquare(Color::White);
        fp.blackKingSquare = pos.getKingSquare(Color::Black);
        fp.hashKey = pos.getHashKey();
        return fp;
    }

    /// \brief Bit-for-bit equality check across all captured state variables.
    bool operator==(const PositionFingerprint& rhs) const noexcept {
        return pieces == rhs.pieces
            && whiteOccupancy == rhs.whiteOccupancy
            && blackOccupancy == rhs.blackOccupancy
            && totalOccupancy == rhs.totalOccupancy
            && sideToMove == rhs.sideToMove
            && castlingRights == rhs.castlingRights
            && enPassantSquare == rhs.enPassantSquare
            && halfmoveClock == rhs.halfmoveClock
            && fullmoveNumber == rhs.fullmoveNumber
            && whiteKingSquare == rhs.whiteKingSquare
            && blackKingSquare == rhs.blackKingSquare
            && hashKey == rhs.hashKey;
    }

    /// \brief Formats a detailed discrepancy report if any field differs.
    std::string diff(const PositionFingerprint& other) const {
        std::ostringstream oss;
        bool hasDiff = false;

        static const char* pieceNames[12] = {
            "WhitePawn", "WhiteKnight", "WhiteBishop", "WhiteRook", "WhiteQueen", "WhiteKing",
            "BlackPawn", "BlackKnight", "BlackBishop", "BlackRook", "BlackQueen", "BlackKing"
        };

        for (size_t p = 0; p < 12; ++p) {
            if (pieces[p] != other.pieces[p]) {
                hasDiff = true;
                oss << "  [PieceMismatch] " << pieceNames[p]
                    << ": expected 0x" << std::hex << pieces[p]
                    << ", actual 0x" << other.pieces[p] << std::dec << "\n";
            }
        }

        if (whiteOccupancy != other.whiteOccupancy) {
            hasDiff = true;
            oss << "  [WhiteOccupancy] expected 0x" << std::hex << whiteOccupancy
                << ", actual 0x" << other.whiteOccupancy << std::dec << "\n";
        }

        if (blackOccupancy != other.blackOccupancy) {
            hasDiff = true;
            oss << "  [BlackOccupancy] expected 0x" << std::hex << blackOccupancy
                << ", actual 0x" << other.blackOccupancy << std::dec << "\n";
        }

        if (sideToMove != other.sideToMove) {
            hasDiff = true;
            oss << "  [SideToMove] expected " << (sideToMove == Color::White ? "White" : "Black")
                << ", actual " << (other.sideToMove == Color::White ? "White" : "Black") << "\n";
        }

        if (castlingRights != other.castlingRights) {
            hasDiff = true;
            oss << "  [CastlingRights] expected 0x" << std::hex << static_cast<uint8_t>(castlingRights)
                << ", actual 0x" << static_cast<uint8_t>(other.castlingRights) << std::dec << "\n";
        }

        if (enPassantSquare != other.enPassantSquare) {
            hasDiff = true;
            oss << "  [EnPassantSquare] expected " << static_cast<int>(enPassantSquare)
                << ", actual " << static_cast<int>(other.enPassantSquare) << "\n";
        }

        if (halfmoveClock != other.halfmoveClock) {
            hasDiff = true;
            oss << "  [HalfmoveClock] expected " << halfmoveClock
                << ", actual " << other.halfmoveClock << "\n";
        }

        if (fullmoveNumber != other.fullmoveNumber) {
            hasDiff = true;
            oss << "  [FullmoveNumber] expected " << fullmoveNumber
                << ", actual " << other.fullmoveNumber << "\n";
        }

        if (hashKey != other.hashKey) {
            hasDiff = true;
            oss << "  [HashKey] expected 0x" << std::hex << hashKey
                << ", actual 0x" << other.hashKey << std::dec << "\n";
        }

        if (whiteKingSquare != other.whiteKingSquare || blackKingSquare != other.blackKingSquare) {
            hasDiff = true;
            oss << "  [KingSquare] W: " << static_cast<int>(whiteKingSquare) << " vs " << static_cast<int>(other.whiteKingSquare)
                << ", B: " << static_cast<int>(blackKingSquare) << " vs " << static_cast<int>(other.blackKingSquare) << "\n";
        }

        if (!hasDiff) {
            return "No discrepancies detected.";
        }
        return oss.str();
    }
};

} // namespace Boson

#endif // BOSON_POSITION_FINGERPRINT_HPP
