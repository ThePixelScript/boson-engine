#include "board/MoveGenerator.hpp"
#include "board/UndoState.hpp"
#include "board/MoveExecutor.hpp"
#include <bit>
#include <cmath>
#include <cstdlib>
#include <intrin.h>
#include <string>
#include <vector>
#include <iostream>

inline unsigned long countTrailingZeros(uint64_t v) {
    unsigned long index = 0;
    if (_BitScanForward64(&index, v)) return index;
    return 64;
}

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

Bitboard MoveGenerator::calculateSlidingAttacks(Square sq, Bitboard occupancy, [[maybe_unused]] const std::array<Bitboard, 4>& rays, [[maybe_unused]] const std::array<int, 4>& shifts, bool isRook) noexcept {
    Bitboard attacks = 0ULL;
    const size_t sqIdx = static_cast<size_t>(sq);

    for (int dir = 0; dir < 4; ++dir) {
        Bitboard ray = isRook ? s_rookRays[sqIdx][dir] : s_bishopRays[sqIdx][dir];
        Bitboard blockers = ray & occupancy;
        
        if (blockers) {
            unsigned long blockerSq = 0;
            bool forward = isRook ? (dir == 0 || dir == 2) : (dir == 0 || dir == 1);

            if (forward) {
                #if defined(_MSC_VER)
                    _BitScanForward64(&blockerSq, blockers);
                #else
                    blockerSq = countTrailingZeros(blockers);
                #endif
            } else {
                #if defined(_MSC_VER)
                    _BitScanReverse64(&blockerSq, blockers);
                #else
                    blockerSq = 63 - __builtin_clzll(blockers);
                #endif
            }
            Bitboard tail = isRook ? s_rookRays[blockerSq][dir] : s_bishopRays[blockerSq][dir];
            ray &= ~tail;        }
        attacks |= ray;
    }
    return attacks;
}

Bitboard MoveGenerator::getRookAttacks(Square sq, Bitboard occupancy) noexcept {
    static constexpr std::array<int, 4> dummyShifts = {0, 0, 0, 0};
    return calculateSlidingAttacks(sq, occupancy, s_rookRays[static_cast<size_t>(sq)], dummyShifts, true);
}

Bitboard MoveGenerator::getBishopAttacks(Square sq, Bitboard occupancy) noexcept {
    static constexpr std::array<int, 4> dummyShifts = {0, 0, 0, 0};
    return calculateSlidingAttacks(sq, occupancy, s_bishopRays[static_cast<size_t>(sq)], dummyShifts, false);
}

Bitboard MoveGenerator::getQueenAttacks(Square sq, Bitboard occupancy) noexcept {
    return getRookAttacks(sq, occupancy) | getBishopAttacks(sq, occupancy);
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
                sq = countTrailingZeros(pieceBb);
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
                    targetSq = countTrailingZeros(validMoves);
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
    const Bitboard totalOccupancy = pos.getTotalOccupancy();
    Bitboard king = pos.getPieceBitboard((us == Color::White) ? Piece::WhiteKing : Piece::BlackKing);
    if (king) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, king);
        #else
            sq = countTrailingZeros(king);
        #endif
        
        Bitboard validMoves = s_kingAttacks[sq] & ~friendlyOccupancy;
        while (validMoves) {
            unsigned long targetSq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&targetSq, validMoves);
            #else
                targetSq = countTrailingZeros(validMoves);
            #endif
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSq)));
            validMoves &= validMoves - 1;
        }

        CastlingRights rights = pos.getCastlingRights();
        if (us == Color::White) {
            if (static_cast<bool>(rights & CastlingRights::WhiteOO)) {
                if (!(totalOccupancy & (Bitboards::getSquareBit(Square::F1) | Bitboards::getSquareBit(Square::G1)))) {
                    moves.push_back(Move(Square::E1, Square::G1, Move::Flags::Castling));
                }
            }
            if (static_cast<bool>(rights & CastlingRights::WhiteOOO)) {
                if (!(totalOccupancy & (Bitboards::getSquareBit(Square::D1) | Bitboards::getSquareBit(Square::C1) | Bitboards::getSquareBit(Square::B1)))) {
                    moves.push_back(Move(Square::E1, Square::C1, Move::Flags::Castling));
                }
            }
        } else {
            if (static_cast<bool>(rights & CastlingRights::BlackOO)) {
                if (!(totalOccupancy & (Bitboards::getSquareBit(Square::F8) | Bitboards::getSquareBit(Square::G8)))) {
                    if (!isSquareAttacked(pos, Square::E8, Color::White) && 
                        !isSquareAttacked(pos, Square::F8, Color::White) && 
                        !isSquareAttacked(pos, Square::G8, Color::White)) {
                        moves.push_back(Move(Square::E8, Square::G8, Move::Flags::Castling));
                    }
                }
            }
            if (static_cast<bool>(rights & CastlingRights::BlackOOO)) {
                if (!(totalOccupancy & (Bitboards::getSquareBit(Square::D8) | Bitboards::getSquareBit(Square::C8) | Bitboards::getSquareBit(Square::B8)))) {
                    // Correct check for Queenside: E8, D8, C8
                    if (!isSquareAttacked(pos, Square::E8, Color::White) && 
                        !isSquareAttacked(pos, Square::D8, Color::White) && 
                        !isSquareAttacked(pos, Square::C8, Color::White)) {
                        moves.push_back(Move(Square::E8, Square::C8, Move::Flags::Castling));
                    }
                }
            }
        }
    }
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
            sq = countTrailingZeros(knights);
        #endif

        Bitboard validMoves = s_knightAttacks[sq] & ~friendlyOccupancy;
        while (validMoves) {
            unsigned long targetSq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&targetSq, validMoves);
            #else
                targetSq = countTrailingZeros(validMoves);
            #endif
            moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSq)));
            validMoves &= validMoves - 1;
        }
        knights &= knights - 1;
    }
}

void MoveGenerator::generatePawnMoves(const Position& pos, MoveList& moves) noexcept {
    const Color us = pos.getSideToMove();
    const Bitboard pawns = pos.getPieceBitboard((us == Color::White) ? Piece::WhitePawn : Piece::BlackPawn);
    const Bitboard enemyOccupancy = pos.getColorOccupancy(!us);
    
    if (us == Color::White) {
        generatePawnPushes(pos, pawns, 8, 0xFF00ULL, moves);
        generatePawnCaptures(pos, pawns, 8, enemyOccupancy, moves);
    } else {
        generatePawnPushes(pos, pawns, -8, 0xFF000000000000ULL, moves);
        generatePawnCaptures(pos, pawns, -8, enemyOccupancy, moves);
    }
}

void MoveGenerator::generatePawnPushes(const Position& pos, Bitboard pawns, int direction, uint64_t startRankMask, MoveList& moves) noexcept {
    const Bitboard totalOccupancy = pos.getTotalOccupancy();
    while (pawns) {
        unsigned long sq = countTrailingZeros(pawns); // Simplified for clarity
        int target = static_cast<int>(sq) + direction;
        Bitboard targetMask = 1ULL << target;

        if (!(totalOccupancy & targetMask)) {
            if (target >= 56 || target <= 7) {
                // ALL 4 PROMOTIONS
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Queen));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Rook));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Bishop));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Knight));
            } else {
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target)));
                if ((1ULL << sq) & startRankMask) {
                    int doubleTarget = target + direction;
                    if (!(totalOccupancy & (1ULL << doubleTarget)))
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(doubleTarget), Move::Flags::DoublePawnPush));
                }
            }
        }
        pawns &= pawns - 1;
    }
}

void MoveGenerator::generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept {
    const Square epSquare = pos.getEnPassantSquare();
    Bitboard targetMask = enemyOccupancy;
    if (epSquare != Square::None) targetMask |= Bitboards::getSquareBit(epSquare);

    while (pawns) {
        unsigned long sq = countTrailingZeros(pawns);
        Bitboard pawnBit = 1ULL << sq;
        // Directional masks (simplified)
        Bitboard captures = ((direction == 8) ? ((pawnBit & 0xFEFEFEFEFEFEFEFEULL) << 7 | (pawnBit & 0x7F7F7F7F7F7F7F7FULL) << 9) 
                                              : ((pawnBit & 0xFEFEFEFEFEFEFEFEULL) >> 9 | (pawnBit & 0x7F7F7F7F7F7F7F7FULL) >> 7)) & targetMask;
        
        while(captures) {
            unsigned long target = countTrailingZeros(captures);
            Move::Flags flag = (static_cast<Square>(target) == epSquare) ? Move::Flags::EnPassant : Move::Flags::None;
            
            if (target >= 56 || target <= 7) {
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Queen));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Rook));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Bishop));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), Move::Flags::Promotion, Move::PromotionPiece::Knight));
            } else {
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(target), flag));
            }
            captures &= captures - 1;
        }
        pawns &= pawns - 1;
    }
}

bool MoveGenerator::isSquareAttacked(const Position& pos, Square sq, Color attacker) noexcept {
    initializeTables();
    
    const Bitboard totalOcc = pos.getTotalOccupancy();
    const int sqInt = static_cast<int>(sq);

    Bitboard pawns = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhitePawn : Piece::BlackPawn);
    
    // Mathematically corrected file bounds tracking
    if (attacker == Color::White) {
        if (sqInt % 8 != 7 && sqInt >= 7 && ((1ULL << (sqInt - 7)) & pawns)) return true;
        if (sqInt % 8 != 0 && sqInt >= 9 && ((1ULL << (sqInt - 9)) & pawns)) return true;
    } else {
        if (sqInt % 8 != 7 && sqInt <= 54 && ((1ULL << (sqInt + 9)) & pawns)) return true;
        if (sqInt % 8 != 0 && sqInt <= 56 && ((1ULL << (sqInt + 7)) & pawns)) return true;
    }

    Bitboard knights = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteKnight : Piece::BlackKnight);
    if (s_knightAttacks[static_cast<size_t>(sq)] & knights) return true;

    Bitboard king = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteKing : Piece::BlackKing);
    if (s_kingAttacks[static_cast<size_t>(sq)] & king) return true;

    Bitboard diagonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteBishop : Piece::BlackBishop) |
                                 pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    if (getBishopAttacks(sq, totalOcc) & diagonalAttackers) return true;

    Bitboard orthogonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteRook : Piece::BlackRook) |
                                   pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    if (getRookAttacks(sq, totalOcc) & orthogonalAttackers) return true;

    return false;
}

bool MoveGenerator::inCheck(const Position& pos, Color side) noexcept {
    const Square kingSq = pos.getKingSquare(side);
    if (kingSq == Square::None) {
        return false;
    }
    return isSquareAttacked(pos, kingSq, side == Color::White ? Color::Black : Color::White);
}

void MoveGenerator::generateLegalMoves(Position& pos, MoveList& legalMoves) noexcept {
    initializeTables();
    
    const Color us = pos.getSideToMove();
    const Color them = (us == Color::White) ? Color::Black : Color::White;
    
    MoveList pseudoMoves;
    generateKnightMoves(pos, pseudoMoves);
    generateKingMoves(pos, pseudoMoves);
    generatePawnMoves(pos, pseudoMoves);
    generateSlidingMoves(pos, pseudoMoves);

    // Use standard indexing instead of range-based for to avoid missing iterators
    for (int i = 0; i < pseudoMoves.size(); ++i) {
        const Move& move = pseudoMoves[i];
        UndoState undo;

        if (move.isCastling()) {
            if (inCheck(pos, us)) continue;

            Square to = move.getToSquare();
            if (to == Square::G1 && (isSquareAttacked(pos, Square::E1, them) || 
                                    isSquareAttacked(pos, Square::F1, them) || 
                                    isSquareAttacked(pos, Square::G1, them))) continue;
            
            if (to == Square::C1 && (isSquareAttacked(pos, Square::E1, them) || 
                                    isSquareAttacked(pos, Square::D1, them) || 
                                    isSquareAttacked(pos, Square::C1, them))) continue;
                                    
            if (to == Square::G8 && (isSquareAttacked(pos, Square::E8, them) || 
                                    isSquareAttacked(pos, Square::F8, them) || 
                                    isSquareAttacked(pos, Square::G8, them))) continue;
                                    
            if (to == Square::C8 && (isSquareAttacked(pos, Square::E8, them) || 
                                    isSquareAttacked(pos, Square::D8, them) || 
                                    isSquareAttacked(pos, Square::C8, them))) continue;
        }

        MoveExecutor::makeMove(pos, move, undo);
        if (!inCheck(pos, us)) {
            legalMoves.push_back(move);
        }
        MoveExecutor::undoMove(pos, move, undo);
    }
}

void MoveGenerator::generateTacticalMoves(Position& pos, MoveList& moves) noexcept {
    MoveList allMoves;
    generateLegalMoves(pos, allMoves);

    for (size_t i = 0; i < allMoves.size(); ++i) {
        const Move& m = allMoves[i];
        
        Bitboard targetBit = 1ULL << static_cast<size_t>(m.getToSquare());
        bool isNormalCapture = (pos.getTotalOccupancy() & targetBit) != 0;
        bool isEnPassantCapture = (m.getToSquare() == pos.getEnPassantSquare() && pos.getEnPassantSquare() != Square::None);

        if (isNormalCapture || isEnPassantCapture) {
            moves.push_back(m);
        }
    }
}

} // namespace Boson