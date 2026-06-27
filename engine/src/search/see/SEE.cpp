#include "search/see/SEE.hpp"
#include "board/MoveGenerator.hpp"
#include "evaluation/Evaluator.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace Boson {

int SEE::getPieceValue(Piece p) noexcept {
    switch (p) {
        case Piece::WhitePawn:   case Piece::BlackPawn:   return Evaluator::PAWN_VALUE;
        case Piece::WhiteKnight: case Piece::BlackKnight: return Evaluator::KNIGHT_VALUE;
        case Piece::WhiteBishop: case Piece::BlackBishop: return Evaluator::BISHOP_VALUE;
        case Piece::WhiteRook:   case Piece::BlackRook:   return Evaluator::ROOK_VALUE;
        case Piece::WhiteQueen:  case Piece::BlackQueen:  return Evaluator::QUEEN_VALUE;
        case Piece::WhiteKing:   case Piece::BlackKing:   return 99999;
        default: return 0;
    }
}

Bitboard SEE::getAttackers(const Position& pos, Square target, Bitboard occupancy) noexcept {
    Bitboard attackers = 0;
    size_t targetIdx = static_cast<size_t>(target);
    int targetFile = static_cast<int>(targetIdx % 8);
    int targetRank = static_cast<int>(targetIdx / 8);

    // 1. Scan for White Pawns (Must also check if still alive in occupancy mask)
    Bitboard wPawns = pos.getPieceBitboard(Piece::WhitePawn) & occupancy;
    if (targetRank >= 1) {
        if (targetFile > 0) {
            int src = static_cast<int>(targetIdx) - 9;
            if ((1ULL << src) & wPawns) attackers |= (1ULL << src);
        }
        if (targetFile < 7) {
            int src = static_cast<int>(targetIdx) - 7;
            if ((1ULL << src) & wPawns) attackers |= (1ULL << src);
        }
    }

    // 2. Scan for Black Pawns (Must also check occupancy)
    Bitboard bPawns = pos.getPieceBitboard(Piece::BlackPawn) & occupancy;
    if (targetRank <= 6) {
        if (targetFile > 0) {
            int src = static_cast<int>(targetIdx) + 7;
            if ((1ULL << src) & bPawns) attackers |= (1ULL << src);
        }
        if (targetFile < 7) {
            int src = static_cast<int>(targetIdx) + 9;
            if ((1ULL << src) & bPawns) attackers |= (1ULL << src);
        }
    }

    // 3. Knight Attacks (Must also check occupancy)
    static const int knightOffsets[] = { -17, -15, -10, -6, 6, 10, 15, 17 };
    Bitboard knights = (pos.getPieceBitboard(Piece::WhiteKnight) | pos.getPieceBitboard(Piece::BlackKnight)) & occupancy;
    for (int offset : knightOffsets) {
        int srcIdx = static_cast<int>(targetIdx) + offset;
        if (srcIdx >= 0 && srcIdx <= 63) {
            if (std::abs((srcIdx % 8) - targetFile) <= 2 && std::abs((srcIdx / 8) - targetRank) <= 2) {
                if ((1ULL << srcIdx) & knights) attackers |= (1ULL << srcIdx);
            }
        }
    }

    // 4. King Attacks (Must also check occupancy)
    static const int kingOffsets[] = { -9, -8, -7, -1, 1, 7, 8, 9 };
    Bitboard kings = (pos.getPieceBitboard(Piece::WhiteKing) | pos.getPieceBitboard(Piece::BlackKing)) & occupancy;
    for (int offset : kingOffsets) {
        int srcIdx = static_cast<int>(targetIdx) + offset;
        if (srcIdx >= 0 && srcIdx <= 63) {
            if (std::abs((srcIdx % 8) - targetFile) <= 1 && std::abs((srcIdx / 8) - targetRank) <= 1) {
                if ((1ULL << srcIdx) & kings) attackers |= (1ULL << srcIdx);
            }
        }
    }

    // 5. Direct Ray Casting for Slider Attacks (Bishops, Rooks, Queens)
    static const int directions[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0},   // Orthogonal
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  // Diagonal
    };

    Bitboard rooksQueens = pos.getPieceBitboard(Piece::WhiteRook) | pos.getPieceBitboard(Piece::BlackRook) |
                           pos.getPieceBitboard(Piece::WhiteQueen) | pos.getPieceBitboard(Piece::BlackQueen);
    Bitboard bishopsQueens = pos.getPieceBitboard(Piece::WhiteBishop) | pos.getPieceBitboard(Piece::BlackBishop) |
                             pos.getPieceBitboard(Piece::WhiteQueen)  | pos.getPieceBitboard(Piece::BlackQueen);

    for (int d = 0; d < 8; ++d) {
        int fStep = directions[d][0];
        int rStep = directions[d][1];
        
        int f = targetFile + fStep;
        int r = targetRank + rStep;
        
        while (f >= 0 && f <= 7 && r >= 0 && r <= 7) {
            int idx = r * 8 + f;
            Bitboard bit = 1ULL << idx;
            
            if (d < 4) {
                if (bit & rooksQueens & occupancy) {
                    attackers |= bit;
                    break;
                }
            } else {
                if (bit & bishopsQueens & occupancy) {
                    attackers |= bit;
                    break;
                }
            }
            
            if (bit & occupancy) {
                break; 
            }
            
            f += fStep;
            r += rStep;
        }
    }

    return attackers;
}

Square SEE::getLeastValuableAttacker(const Position& pos, Bitboard attackers, Color side, Piece& outPiece) noexcept {
    std::array<Piece, 6> whiteOrder = { Piece::WhitePawn, Piece::WhiteKnight, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteQueen, Piece::WhiteKing };
    std::array<Piece, 6> blackOrder = { Piece::BlackPawn, Piece::BlackKnight, Piece::BlackBishop, Piece::BlackRook, Piece::BlackQueen, Piece::BlackKing };

    const auto& order = (side == Color::White) ? whiteOrder : blackOrder;

    for (Piece p : order) {
        Bitboard matchingPieces = attackers & pos.getPieceBitboard(p);
        if (matchingPieces) {
            outPiece = p;
            unsigned long sq = 0;
            #if defined(_MSC_VER)
                _BitScanForward64(&sq, matchingPieces);
            #else
                sq = __builtin_ctzll(matchingPieces);
            #endif
            return static_cast<Square>(sq);
        }
    }
    outPiece = Piece::None;
    return Square::None;
}

int SEE::evaluate(Position& pos, Square fromSq, Square toSq) noexcept {
    std::array<int, 32> gain{};
    int d = 0;

    Bitboard occupancy = pos.getTotalOccupancy();
    
    Piece attacker = Piece::None;
    for (size_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & (1ULL << static_cast<size_t>(fromSq))) {
            attacker = static_cast<Piece>(p);
            break;
        }
    }
    
    Piece victim = Piece::None;
    for (size_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & (1ULL << static_cast<size_t>(toSq))) {
            victim = static_cast<Piece>(p);
            break;
        }
    }
    
    if (victim == Piece::None && (attacker == Piece::WhitePawn || attacker == Piece::BlackPawn) && toSq == pos.getEnPassantSquare()) {
        victim = (pos.getSideToMove() == Color::White) ? Piece::BlackPawn : Piece::WhitePawn;
    }

    gain[0] = getPieceValue(victim);
    Color currentSide = pos.getSideToMove();

    occupancy &= ~(1ULL << static_cast<size_t>(fromSq));
    Bitboard attackers = getAttackers(pos, toSq, occupancy);

    while (attackers) {
        currentSide = (currentSide == Color::White) ? Color::Black : Color::White;
        
        Piece currentAttacker = Piece::None;
        Square attSq = getLeastValuableAttacker(pos, attackers, currentSide, currentAttacker);
        
        if (currentAttacker == Piece::None) break;

        d++; 
        if (d >= 32) break;

        // Save exchange delta tracking score based on the piece being captured
        gain[d] = getPieceValue(attacker) - gain[d - 1];
        attacker = currentAttacker; // Shift up to the current active piece

        occupancy &= ~(1ULL << static_cast<size_t>(attSq));
        attackers = getAttackers(pos, toSq, occupancy);
    }

    // Standard minimax backup routine to resolve optional trading choices
    while (d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
        d--;
    }

    return gain[0];
}

} // namespace Boson