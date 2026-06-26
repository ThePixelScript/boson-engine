#include "board/MoveGenerator.hpp"

namespace Boson {

std::array<Bitboard, 64> MoveGenerator::s_knightAttacks{};
bool MoveGenerator::s_initialized = false;

void MoveGenerator::initializeTables() noexcept {
    if (s_initialized) return;

    // File masks to prevent wrap-around bugs on the edges of the board
    constexpr Bitboard clearA = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard clearAB = 0xFCFCFCFCFCFCFCFCULL;
    constexpr Bitboard clearH = 0x7F7F7F7F7F7F7F7FULL;
    constexpr Bitboard clearHG = 0x3F3F3F3F3F3F3F3FULL;

    for (uint8_t sq = 0; sq < 64; ++sq) {
        Bitboard knightMask = 0ULL;
        Bitboard b = 1ULL << sq;

        // Up 2, Left 1 / Right 1
        if (sq < 48) {
            if (b & clearA)  knightMask |= (b << 15);
            if (b & clearH)  knightMask |= (b << 17);
        }
        // Up 1, Left 2 / Right 2
        if (sq < 56) {
            if (b & clearAB) knightMask |= (b << 6);
            if (b & clearHG) knightMask |= (b << 10);
        }
        // Down 1, Left 2 / Right 2
        if (sq >= 8) {
            if (b & clearAB) knightMask |= (b >> 10);
            if (b & clearHG) knightMask |= (b >> 6);
        }
        // Down 2, Left 1 / Right 1
        if (sq >= 16) {
            if (b & clearA)  knightMask |= (b >> 17);
            if (b & clearH)  knightMask |= (b >> 15);
        }

        s_knightAttacks[sq] = knightMask;
    }

    s_initialized = true;
}

void MoveGenerator::generateKnightMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard friendlyOccupancy = pos.getColorOccupancy(us);
    
    // Identify our knights based on side to move
    const Piece knightPiece = (us == Color::White) ? Piece::WhiteKnight : Piece::BlackKnight;
    Bitboard knights = pos.getPieceBitboard(knightPiece);

    while (knights) {
        // Find least significant bit (position of the active knight)
        // Using common compiler builtins or basic bit tricks for maximum compatibility
        #if defined(_MSC_VER)
            unsigned long sq = 0;
            _BitScanForward64(&sq, knights);
        #else
            unsigned long sq = __builtin_ctzll(knights);
        #endif

        // Fetch pre-calculated raw leap attack bitboard
        Bitboard attacks = s_knightAttacks[sq];
        
        // A legal landing zone cannot contain a friendly piece
        Bitboard validMoves = attacks & ~friendlyOccupancy;

        while (validMoves) {
            #if defined(_MSC_VER)
                unsigned long targetSq = 0;
                _BitScanForward64(&targetSq, validMoves);
            #else
                unsigned long targetSq = __builtin_ctzll(validMoves);
            #endif

            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSq), Move::MoveType::Normal));
            validMoves &= validMoves - 1; // Pop processed target bit
        }

        knights &= knights - 1; // Pop processed knight bit
    }
}

} // namespace Boson