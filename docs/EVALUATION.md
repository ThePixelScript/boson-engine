# Boson Evaluation Architecture

## 1. Overview and Design Contract

The Boson evaluation subsystem computes a heuristic value for a terminal search leaf. It adheres strictly to Immutable Principle 1 (Correctness before Speed) and Principle 4 (Replaceable Modules).

### Evaluation Contract
```cpp
[[nodiscard]] int evaluate(const Position& pos) noexcept;
```

1. **Side-to-Move Relative**: The return value is strictly relative to `pos.getSideToMove()`. A positive score indicates an advantage for the side to move; a negative score indicates a disadvantage.
2. **Centipawn Scale**: All scores are normalized to centipawns ($100\text{ cp} \approx 1\text{ pawn}$ in standard nominal value).
3. **Purity and Invariance**: The evaluation function is deterministic and strictly side-effect free. Given identical positions, `evaluate()` returns the exact same score. It does not mutate the `Position` state, global statistics, or search controllers.
4. **Symmetry**: A mirrored position with opposite side to move must yield an identical evaluation from its own perspective. Symmetrical positions (such as the initial starting array) evaluate to $0\text{ cp}$.

---

## 2. Evaluation Terms Breakdown

The classical HCE (Handcrafted Evaluation) engine decomposes board evaluation into modular additive and tapered terms:

$$\text{Eval} = \left( \text{Score}_{\text{White}} - \text{Score}_{\text{Black}} \right) \times \text{Sign}(\text{SideToMove}) + \text{Correction}(\text{Side}, \text{Hash})$$

### 2.1 Material Values
Base nominal values establish the foundation of the evaluation gradient:
- Pawn: $100\text{ cp}$
- Knight: $300\text{ cp}$
- Bishop: $325\text{ cp}$
- Rook: $500\text{ cp}$
- Queen: $900\text{ cp}$
- King: $0\text{ cp}$ (invaluable)

### 2.2 Piece-Square Tables (PST)
Positional values reflect board geometry, center control, piece coordination, and safety:
- **Pawn**: Encourages center pushes (e4/d4), development, and awards rank advancements ($+50\text{ cp}$ on Rank 7 approaching promotion).
- **Knight**: Penalizes rim/corner placement ($-50\text{ cp}$ on a1/h1/a8/h8) and rewards centralized outposts ($+20\text{ cp}$ on d4/e4/d5/e5).
- **Bishop**: Promotes long open diagonals and avoids corner traps.
- **Rook**: Rewards central files (d1/e1) and penetration to the 7th rank ($+10\text{ cp}$).
- **Queen**: Discourages premature exposure while maintaining centralization.
- **King**:
  - **Middlegame (`KingMiddle`)**: Heavily rewards king safety behind pawn shields (castling to g1/c1: $+20$ to $+30\text{ cp}$) while severely penalizing exposure on advanced ranks ($-50\text{ cp}$).
  - **Endgame (`KingEndgame`)**: Inverts king priority, incentivizing aggressive centralization towards the center ($+40\text{ cp}$) and penalizing corner confinement ($-50\text{ cp}$).

---

## 3. PST Coordinate Layout & Black Mirroring

### 3.1 White-Perspective Layout
Boson bitboards use little-endian rank-file mapping (`A1 = 0`, `H1 = 7`, `A8 = 56`, `H8 = 63`).
All PST arrays in `PieceSquareTables.cpp` are defined canonically from **White's perspective**, matching memory order:
- **Indices 0..7**: Rank 1 ($A1 \dots H1$)
- **Indices 8..15**: Rank 2 ($A2 \dots H2$)
- **Indices 16..23**: Rank 3 ($A3 \dots H3$)
- **Indices 24..31**: Rank 4 ($A4 \dots H4$)
- **Indices 32..39**: Rank 5 ($A5 \dots H5$)
- **Indices 40..47**: Rank 6 ($A6 \dots H6$)
- **Indices 48..55**: Rank 7 ($A7 \dots H7$)
- **Indices 56..63**: Rank 8 ($A8 \dots H8$)

### 3.2 Dynamic Black Rank-Flip Mirroring
Black positional scores are derived dynamically from White's tables using bitwise XOR rank inversion:

$$\text{pstIndex}_{\text{Black}} = \text{sq} \oplus 56$$

For any square with rank $r$ and file $f$:
$$\text{sq} = 8r + f$$
$$\text{sq} \oplus 56 = (8r + f) \oplus (8 \times 7) = 8(7 - r) + f$$

This reflects the rank across the horizontal midline while preserving the file column:
- Black pawn on $E7$ ($52$) maps to index $52 \oplus 56 = 12$ ($E2$ in White's table).
- Black King castled on $G8$ ($62$) maps to index $62 \oplus 56 = 6$ ($G1$ in White's table, receiving $+30\text{ cp}$).
- Black pawn on $E2$ ($12$) maps to index $12 \oplus 56 = 52$ ($E7$ in White's table, receiving $+50\text{ cp}$).

This mathematical equivalence guarantees exact color symmetry without duplicate data tables.

---

## 4. Game Phase Tapering

Chess positions smoothly transition from opening/middlegame structures to sparse endgames. A fixed evaluation profile produces pathological play (e.g. hiding the king in an endgame or centralizing the king in a crowded middlegame).

Boson implements phase tapering using non-pawn material weights:
- Knight: $1$ phase point
- Bishop: $1$ phase point
- Rook: $2$ phase points
- Queen: $4$ phase points

$$\text{Total Phase Weight} = 4 \times 1 + 4 \times 1 + 4 \times 2 + 2 \times 4 = 24$$

The instantaneous phase is calculated from total non-pawn pieces:
$$\text{phase} = \text{clamp}\left(\sum_{\text{knights}} 1 + \sum_{\text{bishops}} 1 + \sum_{\text{rooks}} 2 + \sum_{\text{queens}} 4, 0, 24\right)$$

- $\text{phase} = 24$: Pure middlegame.
- $\text{phase} = 0$: Pure endgame (pawns and kings only).

The King positional value interpolates between `KingMiddle` and `KingEndgame`:

$$\text{KingScore} = \frac{\text{KingMiddle}[\text{idx}] \times \text{phase} + \text{KingEndgame}[\text{idx}] \times (24 - \text{phase})}{24}$$

---

## 5. NNUE Upgrade Path & Modular Isolation

Boson's architecture isolates evaluation behind the `Evaluator` boundary:
1. **Search Decoupling**: The search framework queries `Evaluator::evaluate(pos)` strictly as a read-only black box. Search algorithms (alpha-beta, aspiration windows, LMR, null-move pruning) possess zero assumptions about whether evaluation is computed via classical PST or a neural network.
2. **Zero Mutable Interference**: Evaluation does not mutate position or search structures.
3. **NNUE Compatibility**:
   - `Position` exposes piece bitboards by type and color (`pos.getPieceBitboard(...)`).
   - An NNUE implementation will introduce halfKP or 768-feature accumulators. Because `Position` already tracks incremental changes across `makeMove`/`undoMove`, an `AccumulatorCache` can be hooked into `UndoState` without disturbing the `Evaluator` API boundary.
   - Replacing classical evaluation with an NNUE network requires only substituting `Evaluator::evaluate()` to query the forward inference pass, maintaining 100% interoperability with the existing verification harness.
