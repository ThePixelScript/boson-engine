#include "search/MoveOrderer.hpp"
#include "search/see/SEE.hpp"
#include <algorithm>

namespace Boson {

const std::array<std::array<int, 6>, 6> MoveOrderer::MVV_LVA = {{
    // Victims: P,  N,  B,  R,  Q,  K
    /* Pawn */   {15, 25, 35, 45, 55, 0},
    /* Knight */ {14, 24, 34, 44, 54, 0},
    /* Bishop */ {13, 23, 33, 44, 53, 0},
    /* Rook */   {12, 22, 32, 42, 52, 0},
    /* Queen */  {11, 21, 31, 41, 51, 0},
    /* King */   {10, 20, 30, 40, 50, 0}
}};

Piece MoveOrderer::findPieceAtSquare(const Position& pos, Square sq) noexcept {
    Bitboard targetBit = 1ULL << static_cast<size_t>(sq);
    for (size_t p = 0; p < 12; ++p) {
        if (pos.getPieceBitboard(static_cast<Piece>(p)) & targetBit) {
            return static_cast<Piece>(p);
        }
    }
    return Piece::None;
}

int MoveOrderer::getPieceIndex(Piece p) noexcept {
    switch (p) {
        case Piece::WhitePawn:   case Piece::BlackPawn:   return 0;
        case Piece::WhiteKnight: case Piece::BlackKnight: return 1;
        case Piece::WhiteBishop: case Piece::BlackBishop: return 2;
        case Piece::WhiteRook:   case Piece::BlackRook:   return 3;
        case Piece::WhiteQueen:  case Piece::BlackQueen:  return 4;
        case Piece::WhiteKing:   case Piece::BlackKing:   return 5;
        default:                                          return 0;
    }
}

void MoveOrderer::scoreAndSortMoves(
    const Position& pos, 
    MoveList& moves, 
    Move ttMove, 
    const std::array<std::array<Move, 2>, 64>& killerMoves,
    const std::array<std::array<uint32_t, 64>, 12>& historyTable,
    int ply
) noexcept {
    std::array<int, 256> scores{};

    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        int score = SCORE_QUIET;

        if (m.getRawData() == ttMove.getRawData() && ttMove.getRawData() != 0) {
            score = SCORE_TT;
        }
        else {
            Piece attacker = findPieceAtSquare(pos, m.getFromSquare());
            Piece victim = findPieceAtSquare(pos, m.getToSquare());
            
            // Check if it's an En Passant capture (Target square is empty but it captures a pawn)
            bool isEnPassant = false;
            if (attacker == Piece::WhitePawn && pos.getEnPassantSquare() == m.getToSquare() && m.getToSquare() != Square::None) {
                victim = Piece::BlackPawn;
                isEnPassant = true;
            } else if (attacker == Piece::BlackPawn && pos.getEnPassantSquare() == m.getToSquare() && m.getToSquare() != Square::None) {
                victim = Piece::WhitePawn;
                isEnPassant = true;
            }

            // If a victim piece exists, it's a capture node
            if (victim != Piece::None) {
                int aIdx = getPieceIndex(attacker);
                int vIdx = getPieceIndex(victim);
                int mvvLvaScore = SCORE_CAPTURES + MVV_LVA[aIdx][vIdx];

                // Module 6.5 Integration: Evaluate tactical safety using SEE
                // We cast const away safely since SEE works on a local copy of occupancy bitboards
                int seeValue = SEE::evaluate(const_cast<Position&>(pos), m.getFromSquare(), m.getToSquare());

                if (seeValue < 0) {
                    // It's a losing capture trap! Demote it drastically so it's searched AFTER good quiet moves
                    score = SCORE_QUIET - 10000 + seeValue;
                } else {
                    // Safe or winning capture, maintain high MVV-LVA sort priority
                    score = mvvLvaScore;
                }
            }
            // Fallback checking for promotions based on pawn destination ranks
            else if ((attacker == Piece::WhitePawn && m.getToSquare() >= Square::A8 && m.getToSquare() <= Square::H8) ||
                     (attacker == Piece::BlackPawn && m.getToSquare() >= Square::A1 && m.getToSquare() <= Square::H1)) {
                score = SCORE_PROMOTIONS;
            }
            // Quiet Moves (Killer / History)
            else {
                if (ply < 64) {
                    if (m.getRawData() == killerMoves[ply][0].getRawData()) {
                        score = SCORE_KILLER_1;
                    } else if (m.getRawData() == killerMoves[ply][1].getRawData()) {
                        score = SCORE_KILLER_2;
                    } else if (attacker != Piece::None) {
                        size_t pIdx = static_cast<size_t>(attacker);
                        size_t toIdx = static_cast<size_t>(m.getToSquare());
                        score = static_cast<int>(historyTable[pIdx][toIdx]);
                    }
                }
            }
        }
        scores[i] = score;
    }

    // In-place bubble sort utilizing our new mutable indexing operators
    for (size_t i = 0; i < moves.size(); ++i) {
        for (size_t j = i + 1; j < moves.size(); ++j) {
            if (scores[j] > scores[i]) {
                std::swap(scores[i], scores[j]);
                std::swap(moves[i], moves[j]);
            }
        }
    }
}

} // namespace Boson