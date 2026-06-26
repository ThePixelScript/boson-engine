#include "board/MoveGenerator.hpp"
#include <bit>

namespace Boson {

std::array<Bitboard, 64> MoveGenerator::s_knightAttacks{};
std::array<Bitboard, 64> MoveGenerator::s_kingAttacks{};
std::array<std::array<Bitboard, 4>, 64> MoveGenerator::s_rookRays{};
std::array<std::array<Bitboard, 4>, 64> MoveGenerator::s_bishopRays{};
bool MoveGenerator::s_initialized = false;

void MoveGenerator::initializeTables() noexcept {
    if (s_initialized) return;

    constexpr Bitboard clearA = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard clearAB = 0xFCFCFCFCFCFCFCFCULL;
    constexpr Bitboard clearH = 0x7F7F7F7F7F7F7F7FULL;
    constexpr Bitboard clearHG = 0x3F3F3F3F3F3F3F3FULL;

    for (uint8_t sq = 0; sq < 64; ++sq) {
        Bitboard b = 1ULL << sq;
        
        Bitboard kMask = 0ULL;
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

        Bitboard kingMask = 0ULL;
        if (b & clearA)  kingMask |= (b << 7) | (b >> 1) | (b >> 9);
        if (b & clearH)  kingMask |= (b << 9) | (b << 1) | (b >> 7);
        kingMask |= (b << 8) | (b >> 8);
        s_kingAttacks[sq] = kingMask;

        int r = sq / 8;
        int f = sq % 8;

        // Rook Rays (0=N, 1=S, 2=E, 3=W)
        for (int i = r + 1; i < 8; ++i) s_rookRays[sq][0] |= (1ULL << (i * 8 + f));
        for (int i = r - 1; i >= 0; --i) s_rookRays[sq][1] |= (1ULL << (i * 8 + f));
        for (int i = f + 1; i < 8; ++i) s_rookRays[sq][2] |= (1ULL << (r * 8 + i));
        for (int i = f - 1; i >= 0; --i) s_rookRays[sq][3] |= (1ULL << (r * 8 + i));

        // Bishop Rays (0=NE, 1=NW, 2=SE, 3=SW)
        for (int i = 1; r + i < 8 && f + i < 8; ++i) s_bishopRays[sq][0] |= (1ULL << ((r + i) * 8 + (f + i)));
        for (int i = 1; r + i < 8 && f - i >= 0; ++i) s_bishopRays[sq][1] |= (1ULL << ((r + i) * 8 + (f - i)));
        for (int i = 1; r - i >= 0 && f + i < 8; ++i) s_bishopRays[sq][2] |= (1ULL << ((r - i) * 8 + (f + i)));
        for (int i = 1; r - i >= 0 && f - i >= 0; ++i) s_bishopRays[sq][3] |= (1ULL << ((r - i) * 8 + (f - i)));
    }

    s_initialized = true;
}

Bitboard MoveGenerator::calculateSlidingAttacks([[maybe_unused]] Square sq, Bitboard occupancy, const std::array<Bitboard, 4>& rays, const std::array<int, 4>& shifts, bool isRook) noexcept {    Bitboard attacks = 0ULL;

    for (int dir = 0; dir < 4; ++dir) {
        Bitboard ray = rays[dir];
        Bitboard blockers = ray & occupancy;

        if (!blockers) {
            attacks |= ray;
        } else {
            unsigned long blockerSq = 0;
            if (shifts[dir] > 0) {
                #if defined(_MSC_VER)
                    _BitScanForward64(&blockerSq, blockers);
                #else
                    blockerSq = __builtin_ctzll(blockers);
                #endif
                Bitboard trailingRay = isRook ? s_rookRays[blockerSq][dir] : s_bishopRays[blockerSq][dir];
                attacks |= (ray & ~trailingRay);
            } else {
                #if defined(_MSC_VER)
                    _BitScanReverse64(&blockerSq, blockers);
                #else
                    blockerSq = 63 - __builtin_clzll(blockers);
                #endif
                Bitboard trailingRay = isRook ? s_rookRays[blockerSq][dir] : s_bishopRays[blockerSq][dir];
                attacks |= (ray & ~trailingRay);
            }
            attacks |= (1ULL << blockerSq); 
        }
    }
    return attacks;
}

Bitboard MoveGenerator::getRookAttacks(Square sq, Bitboard occupancy) noexcept {
    constexpr std::array<int, 4> shifts = {1, -1, 1, -1};
    return calculateSlidingAttacks(sq, occupancy, s_rookRays[static_cast<size_t>(sq)], shifts, true);
}

Bitboard MoveGenerator::getBishopAttacks(Square sq, Bitboard occupancy) noexcept {
    constexpr std::array<int, 4> shifts = {1, 1, -1, -1};
    return calculateSlidingAttacks(sq, occupancy, s_bishopRays[static_cast<size_t>(sq)], shifts, false);
}

void MoveGenerator::generateSlidingMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard friendlyOccupancy = pos.getColorOccupancy(us);
    const Bitboard totalOccupancy = pos.getTotalOccupancy();

    std::array<Piece, 3> piecesToGen = {
        (us == Color::White) ? Piece::WhiteRook : Piece::BlackRook,
        (us == Color::White) ? Piece::WhiteBishop : Piece::BlackBishop,
        (us == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen
    };

    for (int i = 0; i < 3; ++i) {
        Bitboard pieceBb = pos.getPieceBitboard(piecesToGen[i]);
        while (pieceBb) {
            unsigned long sq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&sq, pieceBb);
            #else
                sq = __builtin_ctzll(pieceBb);
            #endif

            Bitboard attacks = 0ULL;
            if (i == 0)      attacks = getRookAttacks(static_cast<Square>(sq), totalOccupancy);
            else if (i == 1) attacks = getBishopAttacks(static_cast<Square>(sq), totalOccupancy);
            else             attacks = getQueenAttacks(static_cast<Square>(sq), totalOccupancy);

            Bitboard validMoves = attacks & ~friendlyOccupancy;
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
            pieceBb &= pieceBb - 1;
        }
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
        generatePawnPushes(pos, pawns, 8, 0x000000000000FF00ULL, moves);
        generatePawnCaptures(pos, pawns, 8, enemyOccupancy, moves);
    } else {
        Bitboard pawns = pos.getPieceBitboard(Piece::BlackPawn);
        generatePawnPushes(pos, pawns, -8, 0x00FF000000000000ULL, moves);
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
        int singlePushTarget = static_cast<int>(sq) + direction;
        Bitboard singlePushMask = 1ULL << singlePushTarget;
        if (!(totalOccupancy & singlePushMask)) {
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget)));
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

void MoveGenerator::generatePawnCaptures([[maybe_unused]] const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept {
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
        if (direction == 8) {
            if (pawnBit & clearA) {
                int target = static_cast<int>(sq) + 7;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
            if (pawnBit & clearH) {
                int target = static_cast<int>(sq) + 9;
                if (enemyOccupancy & (1ULL << target)) moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
            }
        } else {
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