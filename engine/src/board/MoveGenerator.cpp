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

Bitboard MoveGenerator::calculateSlidingAttacks([[maybe_unused]] Square sq, Bitboard occupancy, [[maybe_unused]] const std::array<Bitboard, 4>& rays, [[maybe_unused]] const std::array<int, 4>& shifts, bool isRook) noexcept {
    Bitboard attacks = 0ULL;
    const int sqInt = static_cast<int>(sq);

    // Explicit coordinate offsets for step calculations
    // Rook:   0=North (+8), 1=South (-8), 2=East (+1), 3=West (-1)
    // Bishop: 0=NE (+9),    1=NW (+7),    2=SE (-7),   3=SW (-9)
    std::array<int, 4> stepOffsets;
    if (isRook) {
        stepOffsets = {8, -8, 1, -1};
    } else {
        stepOffsets = {9, 7, -7, -9};
    }

    for (int dir = 0; dir < 4; ++dir) {
        int currentSq = sqInt;
        int step = stepOffsets[dir];

        while (true) {
            int nextSq = currentSq + step;
            
            // 1. Boundary check: ensure the square is strictly on the 8x8 board
            if (nextSq < 0 || nextSq >= 64) break;
            
            // 2. Prevent wrapping past edge columns (File A to File H leaks)
            int currFile = currentSq % 8;
            int nextFile = nextSq % 8;
            if (std::abs(currFile - nextFile) > 2) break; 

            attacks |= (1ULL << nextSq);

            // 3. Stop if we encounter an obstacle/occupancy bit
            if (occupancy & (1ULL << nextSq)) break;

            currentSq = nextSq;
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
    const int sqInt = static_cast<int>(sq);

    // 1. Pawn Attacks (Look backwards to find attacking pawns)
    Bitboard pawns = pos.getPieceBitboard((attacker == Color::White) ? Piece::WhitePawn : Piece::BlackPawn);
    
    if (attacker == Color::White) {
        // White pawns attack upwards (from lower indices to higher indices)
        // So a square is attacked if there is a White pawn down-left or down-right
        if (sqInt % 8 != 0 && sqInt >= 7) {
            if ((1ULL << (sqInt - 7)) & pawns) return true;
        }
        if (sqInt % 8 != 7 && sqInt >= 9) {
            if ((1ULL << (sqInt - 9)) & pawns) return true;
        }
    } else {
        // Black pawns attack downwards (from higher indices to lower indices)
        // So a square is attacked if there is a Black pawn up-left or up-right
        if (sqInt % 8 != 0 && sqInt <= 54) { // Max valid source index is 54+9 = 63
            if ((1ULL << (sqInt + 9)) & pawns) return true;
        }
        if (sqInt % 8 != 7 && sqInt <= 56) { // Max valid source index is 56+7 = 63
            if ((1ULL << (sqInt + 7)) & pawns) return true;
        }
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
    const Color them = (us == Color::White) ? Color::Black : Color::White;
    
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
            if (inCheck(pos, us)) continue;
            
            Square to = move.getToSquare();
            if (to == Square::G1) {
                if (isSquareAttacked(pos, Square::F1, them) || isSquareAttacked(pos, Square::G1, them)) continue;
            } else if (to == Square::C1) {
                if (isSquareAttacked(pos, Square::D1, them) || isSquareAttacked(pos, Square::C1, them)) continue;
            } else if (to == Square::G8) {
                if (isSquareAttacked(pos, Square::F8, them) || isSquareAttacked(pos, Square::G8, them)) continue;
            } else if (to == Square::C8) {
                if (isSquareAttacked(pos, Square::D8, them) || isSquareAttacked(pos, Square::C8, them)) continue;
            }
        }

        // Apply state mutation (Flips side to move internally)
        MoveExecutor::makeMove(pos, move, undo);
        
        // Explicitly verify king safety using direct bitboard validation
        bool kingSafe = true;
        Piece myKing = (us == Color::White) ? Piece::WhiteKing : Piece::BlackKing;
        Bitboard myKingBb = pos.getPieceBitboard(myKing);
        
        if (myKingBb) {
            unsigned long kSq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&kSq, myKingBb);
            #else
                kSq = __builtin_ctzll(myKingBb);
            #endif
            
            // Query attacks using explicit opponent color parameter
            if (isSquareAttacked(pos, static_cast<Square>(kSq), them)) {
                kingSafe = false;
            }
        }

        if (kingSafe) {
            legalMoves.push_back(move);
        }

        // Restore universe back to exact initial baseline
        MoveExecutor::undoMove(pos, move, undo);
    }
}

void MoveGenerator::generateTacticalMoves(Position& pos, MoveList& moves) noexcept {
    MoveList allMoves;
    generateLegalMoves(pos, allMoves);

    // Filter only tactical interactions: Normal captures and En Passant captures
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