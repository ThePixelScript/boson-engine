#include "evaluation/PieceSquareTables.hpp"

namespace Boson {

// Positional preference arrays defined strictly from White's perspective.
// Indexing maps directly to Boson Square enum:
//   Row 0: indices 0..7   = Rank 1 (A1..H1)
//   Row 1: indices 8..15  = Rank 2 (A2..H2)
//   ...
//   Row 6: indices 48..55 = Rank 7 (A7..H7)
//   Row 7: indices 56..63 = Rank 8 (A8..H8)
//
// Black pieces dynamically mirror White's perspective via rank-flip: sq ^ 56.

const std::array<int16_t, 64> PieceSquareTables::Pawn = {
      0,  0,  0,  0,  0,  0,  0,  0, // Rank 1
      5, 10, 10,-20,-20, 10, 10,  5, // Rank 2
      5, -5,-10,  0,  0,-10, -5,  5, // Rank 3
      0,  0,  0, 20, 20,  0,  0,  0, // Rank 4
      5,  5, 10, 25, 25, 10,  5,  5, // Rank 5
     10, 10, 20, 30, 30, 20, 10, 10, // Rank 6
     50, 50, 50, 50, 50, 50, 50, 50, // Rank 7
      0,  0,  0,  0,  0,  0,  0,  0  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::Knight = {
    -50,-40,-30,-30,-30,-30,-40,-50, // Rank 1
    -40,-20,  0,  5,  5,  0,-20,-40, // Rank 2
    -30,  0, 10, 15, 15, 10,  0,-30, // Rank 3
    -30,  5, 15, 20, 20, 15,  5,-30, // Rank 4
    -30,  0, 15, 20, 20, 15,  0,-30, // Rank 5
    -30,  5, 10, 15, 15, 10,  5,-30, // Rank 6
    -40,-20,  0,  0,  0,  0,-20,-40, // Rank 7
    -50,-40,-30,-30,-30,-30,-40,-50  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::Bishop = {
    -20,-10,-10,-10,-10,-10,-10,-20, // Rank 1
    -10,  5,  0,  0,  0,  0,  5,-10, // Rank 2
    -10, 10, 10, 10, 10, 10, 10,-10, // Rank 3
    -10,  0, 10, 10, 10, 10,  0,-10, // Rank 4
    -10,  5,  5, 10, 10,  5,  5,-10, // Rank 5
    -10,  0,  5, 10, 10,  5,  0,-10, // Rank 6
    -10,  0,  0,  0,  0,  0,  0,-10, // Rank 7
    -20,-10,-10,-10,-10,-10,-10,-20  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::Rook = {
      0,  0,  0,  5,  5,  0,  0,  0, // Rank 1
     -5,  0,  0,  0,  0,  0,  0, -5, // Rank 2
     -5,  0,  0,  0,  0,  0,  0, -5, // Rank 3
     -5,  0,  0,  0,  0,  0,  0, -5, // Rank 4
     -5,  0,  0,  0,  0,  0,  0, -5, // Rank 5
     -5,  0,  0,  0,  0,  0,  0, -5, // Rank 6
      5, 10, 10, 10, 10, 10, 10,  5, // Rank 7
      0,  0,  0,  0,  0,  0,  0,  0  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::Queen = {
    -20,-10,-10, -5, -5,-10,-10,-20, // Rank 1
    -10,  0,  5,  0,  0,  0,  0,-10, // Rank 2
    -10,  5,  5,  5,  5,  5,  0,-10, // Rank 3
      0,  0,  5,  5,  5,  5,  0, -5, // Rank 4
     -5,  0,  5,  5,  5,  5,  0, -5, // Rank 5
    -10,  0,  5,  5,  5,  5,  0,-10, // Rank 6
    -10,  0,  0,  0,  0,  0,  0,-10, // Rank 7
    -20,-10,-10, -5, -5,-10,-10,-20  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::KingMiddle = {
     20, 30, 10,  0,  0, 10, 30, 20, // Rank 1
     20, 20,  0,  0,  0,  0, 20, 20, // Rank 2
    -10,-20,-20,-20,-20,-20,-20,-10, // Rank 3
    -20,-30,-30,-40,-40,-30,-30,-20, // Rank 4
    -30,-40,-40,-50,-50,-40,-40,-30, // Rank 5
    -30,-40,-40,-50,-50,-40,-40,-30, // Rank 6
    -30,-40,-40,-50,-50,-40,-40,-30, // Rank 7
    -30,-40,-40,-50,-50,-40,-40,-30  // Rank 8
};

const std::array<int16_t, 64> PieceSquareTables::KingEndgame = {
    -50,-30,-30,-30,-30,-30,-30,-50, // Rank 1
    -30,-30,  0,  0,  0,  0,-30,-30, // Rank 2
    -30,-10, 20, 30, 30, 20,-10,-30, // Rank 3
    -30,-10, 30, 40, 40, 30,-10,-30, // Rank 4
    -30,-10, 30, 40, 40, 30,-10,-30, // Rank 5
    -30,-10, 20, 30, 30, 20,-10,-30, // Rank 6
    -30,-20,-10,  0,  0,-10,-20,-30, // Rank 7
    -50,-40,-30,-20,-20,-30,-40,-50  // Rank 8
};

} // namespace Boson