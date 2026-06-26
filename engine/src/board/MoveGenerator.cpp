#include "board/MoveGenerator.hpp"
#include "board/UndoState.hpp"
#include "board/MoveExecutor.hpp"
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

Bitboard MoveGenerator::calculateSlidingAttacks([[maybe_unused]] Square sq, Bitboard occupancy, const std::array<Bitboard, 4>& rays, const std::array<int, 4>& shifts, bool isRook) noexcept {
    Bitboard attacks = 0ULL;

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
    const Bitboard totalOccupancy = pos.getTotalOccupancy();
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

        // --- Phase K: Castling Requirements Logic ---
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
                    moves.push_back(Move(Square::E8, Square::G8, Move::Flags::Castling));
                }
            }
            if (static_cast<bool>(rights & CastlingRights::BlackOOO)) {
                if (!(totalOccupancy & (Bitboards::getSquareBit(Square::D8) | Bitboards::getSquareBit(Square::C8) | Bitboards::getSquareBit(Square::B8)))) {
                    moves.push_back(Move(Square::E8, Square::C8, Move::Flags::Castling));
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
            // Phase I: Check single push promotion rank bounds
            if (singlePushTarget >= 56 || singlePushTarget <= 7) {
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget), Move::Flags::Promotion, Move::PromotionPiece::Queen));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget), Move::Flags::Promotion, Move::PromotionPiece::Rook));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget), Move::Flags::Promotion, Move::PromotionPiece::Bishop));
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget), Move::Flags::Promotion, Move::PromotionPiece::Knight));
            } else {
                moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(singlePushTarget)));
                
                // Phase C2: Double Push Calculation
                if ((1ULL << sq) & startRankMask) {
                    int doublePushTarget = singlePushTarget + direction;
                    if (!(totalOccupancy & (1ULL << doublePushTarget))) {
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(doublePushTarget), Move::Flags::DoublePawnPush));
                    }
                }
            }
        }
        pawns &= pawns - 1;
    }
}

void MoveGenerator::generatePawnCaptures(const Position& pos, Bitboard pawns, int direction, Bitboard enemyOccupancy, MoveList& moves) noexcept {
    constexpr Bitboard clearA = 0xFEFEFEFEFEFEFEFEULL;
    constexpr Bitboard clearH = 0x7F7F7F7F7F7F7F7FULL;
    const Square epSquare = pos.getEnPassantSquare();
    
    // Combine regular enemies and dynamic En Passant targets for tracking
    Bitboard targetMask = enemyOccupancy;
    if (epSquare != Square::None) {
        targetMask |= Bitboards::getSquareBit(epSquare);
    }

    while (pawns) {
        unsigned long sq = 0;
        #if defined(_MSC_VER)
            _BitScanForward64(&sq, pawns);
        #else
            sq = __builtin_ctzll(pawns);
        #endif
        
        Bitboard pawnBit = 1ULL << sq;
        
        auto checkAndEmitCapture = [&](int targetSquare, Bitboard edgeClearMask) {
            if (pawnBit & edgeClearMask) {
                if (targetMask & (1ULL << targetSquare)) {
                    Move::Flags flag = (static_cast<Square>(targetSquare) == epSquare) ? Move::Flags::EnPassant : Move::Flags::None;
                    
                    // Phase I: Capture Promotion Split Check
                    if (targetSquare >= 56 || targetSquare <= 7) {
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSquare), Move::Flags::Promotion, Move::PromotionPiece::Queen));
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSquare), Move::Flags::Promotion, Move::PromotionPiece::Rook));
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSquare), Move::Flags::Promotion, Move::PromotionPiece::Bishop));
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSquare), Move::Flags::Promotion, Move::PromotionPiece::Knight));
                    } else {
                        moves.push_back(Move(static_cast<Square>(sq), static_cast<Square>(targetSquare), flag));
                    }
                }
            }
        };

        if (direction == 8) {
            checkAndEmitCapture(static_cast<int>(sq) + 7, clearA);
            checkAndEmitCapture(static_cast<int>(sq) + 9, clearH);
        } else {
            checkAndEmitCapture(static_cast<int>(sq) - 9, clearA);
            checkAndEmitCapture(static_cast<int>(sq) - 7, clearH);
        }
        pawns &= pawns - 1;
    }
}

bool MoveGenerator::isSquareAttacked(const Position& pos, Square sq, Color attacker) noexcept {
    const Bitboard totalOcc = pos.getTotalOccupancy();

// 1. Pawn Attacks (Look backwards using opposing color's capture targets)
    Bitboard pawns = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhitePawn : Piece::BlackPawn);
    int sqInt = static_cast<int>(sq);

    if (attacker == Color::White) {
        if (sqInt >= 7 && ((1ULL << (sqInt - 7)) & pawns) && (sqInt % 8 != 0)) return true;
        if (sqInt >= 9 && ((1ULL << (sqInt - 9)) & pawns) && (sqInt % 8 != 7)) return true;
    } else {
        if (sqInt <= 56 && ((1ULL << (sqInt + 9)) & pawns) && (sqInt % 8 != 0)) return true;
        if (sqInt <= 54 && ((1ULL << (sqInt + 7)) & pawns) && (sqInt % 8 != 7)) return true;
    }

    // 2. Knight Attacks
    Bitboard knights = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteKnight : Piece::BlackKnight);
    if (s_knightAttacks[static_cast<size_t>(sq)] & knights) return true;

    // 3. King Attacks
    Bitboard king = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteKing : Piece::BlackKing);
    if (s_kingAttacks[static_cast<size_t>(sq)] & king) return true;

    // 4. Bishop & Queen Diagonal Attacks
    Bitboard diagonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteBishop : Piece::BlackBishop) |
                                 pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    if (getBishopAttacks(sq, totalOcc) & diagonalAttackers) return true;

    // 5. Rook & Queen Orthogonal Attacks
    Bitboard orthogonalAttackers = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteRook : Piece::BlackRook) |
                                   pos.getPieceBitboard((attacker == Color::White) ? Piece::WhiteQueen : Piece::BlackQueen);
    if (getRookAttacks(sq, totalOcc) & orthogonalAttackers) return true;

    return false;
}

bool MoveGenerator::inCheck(const Position& pos, Color side) noexcept {
    Piece targetKing = (side == Color::White) ? Piece::WhiteKing : Piece::BlackKing;
    Bitboard kingBb = pos.getPieceBitboard(targetKing);
    if (!kingBb) return false;

    unsigned long kingSq = 0;
    #if defined(_MSC_VER)
        _BitScanForward64(&kingSq, kingBb);
    #else
        kingSq = __builtin_ctzll(kingBb);
    #endif

    return isSquareAttacked(pos, static_cast<Square>(kingSq), !side);
}

void MoveGenerator::generateLegalMoves(Position& pos, MoveList& legalMoves) noexcept {
    const Color us = pos.getSideToMove();
    
    // 1. Gather all candidate moves across every piece category
    MoveList pseudoMoves;
    generateKnightMoves(pos, pseudoMoves);
    generateKingMoves(pos, pseudoMoves);
    generatePawnMoves(pos, pseudoMoves);
    generateSlidingMoves(pos, pseudoMoves);

    // 2. Transactionally verify safety of each candidate
    for (size_t i = 0; i < pseudoMoves.size(); ++i) {
        const Move& move = pseudoMoves[i];
        UndoState undo;

        // Special checking gate for Castling moves
        if (move.isCastling()) {
            // Cannot castle if currently in check
            if (inCheck(pos, us)) continue;
            
            [[maybe_unused]] Square from = move.getFromSquare();
            Square to = move.getToSquare();
            
            if (to == Square::G1) { // White Kingside (verify f1 passing square)
                if (isSquareAttacked(pos, Square::F1, !us)) continue;
            } else if (to == Square::C1) { // White Queenside (verify d1 passing square)
                if (isSquareAttacked(pos, Square::D1, !us)) continue;
            } else if (to == Square::G8) { // Black Kingside (verify f8 passing square)
                if (isSquareAttacked(pos, Square::F8, !us)) continue;
            } else if (to == Square::C8) { // Black Queenside (verify d8 passing square)
                if (isSquareAttacked(pos, Square::D8, !us)) continue;
            }
        }

        // Apply state mutation
        MoveExecutor::makeMove(pos, move, undo);
        
        // Keep move if our king is completely unattacked post-execution
        if (!inCheck(pos, us)) {
            legalMoves.push_back(move);
        }

        // Restore universe back to exact initial baseline
        MoveExecutor::undoMove(pos, move, undo);
    }
}

} // namespace Boson