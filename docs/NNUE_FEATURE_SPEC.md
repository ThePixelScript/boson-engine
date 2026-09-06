# HalfKP Feature Contract (Version 1.0)

## 1. Specification Overview
This document specifies the canonical **HalfKP** sparse feature representation utilized in Boson's Neural Network Evaluation (NNUE) subsystem. HalfKP encodes the spatial relationships between a player's king and all non-king pieces on the chessboard.

- **Status**: Frozen as of Phase 7-B.
- **Contract Version**: 1.0.0
- **Total Sparse Features per Perspective**: 40,960 ($64 \times 10 \times 64$)
- **Immutability Directive**: The feature layout, piece codes, square indexing conventions, and perspective transformations defined herein are permanently frozen. Any structural modification constitutes a breaking change requiring network retrain and formal architectural re-versioning.

---

## 2. Spatial Indexing & Coordinate Systems

### 2.1 Square Encoding (Little-Endian Rank-File)
Squares are indexed as 6-bit unsigned integers $[0, 63]$:
$$\text{Square} = \text{Rank} \times 8 + \text{File}$$
where $\text{File} \in [0, 7]$ corresponding to files A through H, and $\text{Rank} \in [0, 7]$ corresponding to ranks 1 through 8.

- Square 0: A1
- Square 7: H1
- Square 56: A8
- Square 63: H8

### 2.2 Canonical Piece Mapping ($0..9$)
Kings are strictly excluded from piece codes as their coordinates serve as the focal perspective anchor. The 10 non-king piece types are canonically mapped to contiguous integer codes:

| Canonical Code | Piece Symbol | Representation | Internal Enum (`Piece`) |
| :---: | :---: | :--- | :--- |
| **0** | `wP` | White Pawn | `Piece::WhitePawn` (0) |
| **1** | `wN` | White Knight | `Piece::WhiteKnight` (1) |
| **2** | `wB` | White Bishop | `Piece::WhiteBishop` (2) |
| **3** | `wR` | White Rook | `Piece::WhiteRook` (3) |
| **4** | `wQ` | White Queen | `Piece::WhiteQueen` (4) |
| **5** | `bP` | Black Pawn | `Piece::BlackPawn` (6) |
| **6** | `bN` | Black Knight | `Piece::BlackKnight` (7) |
| **7** | `bB` | Black Bishop | `Piece::BlackBishop` (8) |
| **8** | `bR` | Black Rook | `Piece::BlackRook` (9) |
| **9** | `bQ` | Black Queen | `Piece::BlackQueen` (10) |

*Note*: Kings (`Piece::WhiteKing` and `Piece::BlackKing`) and empty squares return code `-1` and never generate piece features.

---

## 3. Feature Indexing Formula

Each active feature represents the triad $(\text{KingSquare}, \text{PieceCode}, \text{PieceSquare})$.

### 3.1 Mathematical Formulation
$$\text{Index} = (kSq \times 640) + (pieceCode \times 64) + pSq$$

Where:
- $kSq \in [0, 63]$ is the perspective-adjusted king square.
- $pieceCode \in [0, 9]$ is the perspective-adjusted piece code.
- $pSq \in [0, 63]$ is the perspective-adjusted piece square.
- $\text{Index} \in [0, 40959]$.

### 3.2 Dual-Perspective Transformations

#### White Perspective (`Color::White`)
From White's perspective, coordinates and colors correspond directly to board geometry:
- $kSq = \text{pos.getKingSquare}(\text{Color::White})$
- $pSq = \text{pieceSquare}$
- $pieceCode = \text{canonicalPieceCode}(\text{piece})$

#### Black Perspective (`Color::Black`)
From Black's perspective, rank coordinates are vertically flipped and piece colors are inverted to maintain rotational symmetry:
- $kSq = \text{pos.getKingSquare}(\text{Color::Black}) \oplus 56$ (rank reflection: $r \leftarrow 7 - r$)
- $pSq = \text{pieceSquare} \oplus 56$ (rank reflection: $r \leftarrow 7 - r$)
- $pieceCode = \begin{cases} pieceCode + 5 & \text{if } pieceCode < 5 \text{ (White piece becomes enemy)} \\ pieceCode - 5 & \text{if } pieceCode \ge 5 \text{ (Black piece becomes friendly)} \end{cases}$

---

## 4. Move Semantics & Differential Updates

The differential update oracle enforces that for every legal state transition $\text{Position}_A \xrightarrow{\text{Move}} \text{Position}_B$:
$$(\text{Features}(A) \setminus \text{Removed}) \cup \text{Added} \equiv \text{Features}(B)$$

### 4.1 Update Classifications

1. **Perspective King Moves & Own Castling**:
   - Condition: $\text{KingSquare}_{\text{before}} \ne \text{KingSquare}_{\text{after}}$.
   - Action: Complete accumulator regeneration from scratch. All active features of position $A$ are placed into $\text{Removed}$, and all active features of position $B$ are placed into $\text{Added}$.
2. **Opponent Castling**:
   - The opponent's king moves (not a feature), but the opponent's castling rook relocates:
     * Remove: $(\text{OurKingSq}, \text{RookPiece}, \text{Rook}_{\text{from}})$
     * Add: $(\text{OurKingSq}, \text{RookPiece}, \text{Rook}_{\text{to}})$
3. **Opponent King Captures**:
   - The opponent's king captures one of our pieces on destination square `to`:
     * Remove: $(\text{OurKingSq}, \text{CapturedPiece}, \text{to})$
4. **Quiet Moves**:
   - Remove: $(\text{KingSq}, \text{MovingPiece}, \text{from})$
   - Add: $(\text{KingSq}, \text{MovingPiece}, \text{to})$
5. **Normal Captures**:
   - Remove: $(\text{KingSq}, \text{MovingPiece}, \text{from})$
   - Remove: $(\text{KingSq}, \text{CapturedPiece}, \text{to})$
   - Add: $(\text{KingSq}, \text{MovingPiece}, \text{to})$
6. **En-Passant Captures**:
   - Remove: $(\text{KingSq}, \text{MovingPawn}, \text{from})$
   - Remove: $(\text{KingSq}, \text{CapturedPawn}, \text{victimSquare})$ where $\text{victimSq} = \text{to} \mp 8$
   - Add: $(\text{KingSq}, \text{MovingPawn}, \text{to})$
7. **Promotions (Quiet & Capture $\times$ Q, R, B, N)**:
   - Remove: $(\text{KingSq}, \text{Pawn}, \text{from})$
   - (If capture) Remove: $(\text{KingSq}, \text{CapturedPiece}, \text{to})$
   - Add: $(\text{KingSq}, \text{PromotedPiece}, \text{to})$

---

## 5. Canonical Reference Vectors

### 5.1 Standard Starting Position (`startpos`)
- FEN: `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1`
- **Total Non-King Pieces**: 30 (15 White + 15 Black)
- **Active Features Count**: Exactly 30 per perspective.
- **Symmetry**: `getActiveFeatures(startpos, White) == getActiveFeatures(startpos, Black)`

#### Sample Reference Features (White Perspective, King at E1 = Sq 4)
- White Pawn on A2 (Sq 8, Code 0): $(4 \times 640) + (0 \times 64) + 8 = \mathbf{2568}$
- White Knight on B1 (Sq 1, Code 1): $(4 \times 640) + (1 \times 64) + 1 = \mathbf{2625}$
- White Bishop on C1 (Sq 2, Code 2): $(4 \times 640) + (2 \times 64) + 2 = \mathbf{2690}$
- White Rook on A1 (Sq 0, Code 3): $(4 \times 640) + (3 \times 64) + 0 = \mathbf{2752}$
- White Queen on D1 (Sq 3, Code 4): $(4 \times 640) + (4 \times 64) + 3 = \mathbf{2819}$
- Black Pawn on A7 (Sq 48, Code 5): $(4 \times 640) + (5 \times 64) + 48 = \mathbf{2928}$
- Black Pawn on H7 (Sq 55, Code 5): $(4 \times 640) + (5 \times 64) + 55 = \mathbf{2935}$
- Black Queen on D8 (Sq 59, Code 9): $(4 \times 640) + (9 \times 64) + 59 = \mathbf{3195}$
