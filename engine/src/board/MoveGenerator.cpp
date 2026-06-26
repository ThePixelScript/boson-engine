#include "board/MoveGenerator.hpp"

namespace Boson {

std::array<Bitboard, 64> MoveGenerator::s_knightAttacks{};
std::array<Bitboard, 64> MoveGenerator::s_kingAttacks{};
bool MoveGenerator::s_initialized = false;

void MoveGenerator::initializeTables() noexcept {
    if (s_initialized) return;

    constexpr Bitboard clearA = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard clearAB = 0xFCFCFCFCFCFCFCFCULL;
    constexpr Bitboard clearH = 0x7F7F7F7F7F7F7F7FULL;
    constexpr Bitboard clearHG = 0x3F3F3F3F3F3F3F3FULL;

    for (uint8_t sq = 0; sq < 64; ++sq) {
        Bitboard kMask = 0ULL;
        Bitboard b = 1ULL << sq;

        // --- Knight Offsets ---
        if (sq < 48) {
            if (b & clearA)  kMask |= (b << 15);
            if (b & clearH)  kMask |= (b << 17);
        }
        if (sq < 56) {
            if (b & clearAB) kMask |= (b << 6);
            if (b & clearHG) kMask |= (b << 10);
        }
        if (sq >= 8) {
            if (b & clearAB) kMask |= (b >> 10);
            if (b & clearHG) kMask |= (b >> 6);
        }
        if (sq >= 16) {
            if (b & clearA)  kMask |= (b >> 17);
            if (b & clearH)  kMask |= (b >> 15);
        }
        s_knightAttacks[sq] = kMask;

        // --- King Offsets ---
        Bitboard kingMask = 0ULL;
        if (b & clearA) {
            kingMask |= (b << 7) | (b >> 1) | (b >> 9);
        }
        if (b & clearH) {
            kingMask |= (b << 9) | (b << 1) | (b >> 7);
        }
        kingMask |= (b << 8) | (b >> 8);
        s_kingAttacks[sq] = kingMask;
    }

    s_initialized = true;
}

void MoveGenerator::generateKnightMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard friendlyOccupancy = pos.getColorOccupancy(us);
    Bitboard knights = pos.getPieceBitboard((us == Color::White) ? Piece::WhiteKnight : Piece::BlackKnight);

    while (knights) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, knights);
        #else
            sq = __builtin_ctzll(knights);
        #endif

        Bitboard validMoves = s_knightAttacks[sq] & ~friendlyOccupancy;
        while (validMoves) {
            unsigned long targetSq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&targetSq, validMoves);
            #else
                targetSq = __builtin_ctzll(validMoves);
            #endif
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSq)));
            validMoves &= validMoves - 1;
        }
        knights &= knights - 1;
    }
}

void MoveGenerator::generateKingMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard friendlyOccupancy = pos.getColorOccupancy(us);
    Bitboard king = pos.getPieceBitboard((us == Color::White) ? Piece::WhiteKing : Piece::BlackKing);

    if (king) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, king);
        #else
            sq = __builtin_ctzll(king);
        #endif

        Bitboard validMoves = s_kingAttacks[sq] & ~friendlyOccupancy;
        while (validMoves) {
            unsigned long targetSq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&targetSq, validMoves);
            #else
                targetSq = __builtin_ctzll(validMoves);
            #endif
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSq)));
            validMoves &= validMoves - 1;
        }
    }
}

void MoveGenerator::generatePawnMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard enemyOccupancy = pos.getColorOccupancy(!us);
    
    if (us == Color::White) {
        Bitboard pawns = pos.getPieceBitboard(Piece::WhitePawn);
        generatePawnPushes(pos, pawns, 8, 0x000000000000FF00ULL, moves); // Rank 2
        generatePawnCaptures(pos, pawns, 8, enemyOccupancy, moves);
    } else {
        Bitboard pawns = pos.getPieceBitboard(Piece::BlackPawn);
        generatePawnPushes(pos, pawns, -8, 0x00FF000000000000ULL, moves); // Rank 7
        generatePawnCaptures(pos, pawns, -8, enemyOccupancy, moves);
    }
}

void MoveGenerator::generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept {
    const Bitboard totalOccupancy = pos.getTotalOccupancy();

    while (pawns) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, pawns);
        #else
            sq = __builtin_ctzll(pawns);
        #endif

        // Phase C1: Single Push Calculation
        int singlePushTarget = static_cast<int>(sq) + direction;
        Bitboard singlePushMask = 1ULL << singlePushTarget;

        if (!(totalOccupancy & singlePushMask)) {
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget)));

            // Phase C2: Double Push Calculation (Only if single push square was empty)
            if ((1ULL << sq) & startRankMask) {
                int doublePushTarget = singlePushTarget + direction;
                if (!(totalOccupancy & (1ULL << doublePushTarget))) {
                    moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(doublePushTarget)));
                }
            }
        }
        pawns &= pawns - 1;
    }
}

void MoveGenerator::generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept {
    constexpr Bitboard clearA = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard clearH = 0x7F7F7F7F7F7F7F7FULL;

    while (pawns) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, pawns);
        #else
            sq = __builtin_ctzll(pawns);
        #endif

        Bitboard pawnBit = 1ULL << sq;
        
        // Phase C3: Diagonal Left and Right Capture Math
        if (direction == 8) { // White capturing up
            if (pawnBit & clearA) {
                int target = static_cast<int>(sq) + 7;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
            if (pawnBit & clearH) {
                int target = static_cast<int>(sq) + 9;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
        } else { // Black capturing down
            if (pawnBit & clearA) {
                int target = static_cast<int>(sq) - 9;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
            if (pawnBit & clearH) {
                int target = static_cast<int>(sq) - 7;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
        }
        pawns &= pawns - 1;
    }
}

} // namespace Boson