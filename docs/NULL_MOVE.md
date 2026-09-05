# Null Move Pruning (NMP) Architecture & Zugzwang Safeguards

## 1. The Null Move Hypothesis

Null Move Pruning (NMP) is a forward-pruning technique based on the **null move hypothesis**: in almost all chess positions, passing the turn (making a "null move") is suboptimal compared to playing a legal move.

If a position is so overwhelmingly advantageous that the side to move can pass the turn (yielding the opponent two consecutive moves) and still produce an evaluation exceeding the soft-cutoff threshold $\beta$ on a reduced-depth search, the original position is virtually guaranteed to fail high. Therefore, the current sub-tree can be pruned immediately with score $\ge \beta$, bypassing full legal move generation, move ordering, and recursive sub-tree expansion.

---

## 2. Strict Eligibility Criteria

Because chess contains non-monotonic positions (where passing is better than any legal move), NMP cannot be applied unconditionally. Boson enforces five mandatory eligibility gates prior to executing a null move:

```
                          [ negamax() Entry ]
                                  |
                        +---------v---------+
                        |   allowNull == 1?  |----(No)----> [ Bypass NMP ]
                        +---------+---------+
                                  | (Yes)
                        +---------v---------+
                        |    depth >= 3?     |----(No)----> [ Bypass NMP ]
                        +---------+---------+
                                  | (Yes)
                        +---------v---------+
                        |     !inCheck?     |----(No)----> [ Bypass NMP ]
                        +---------+---------+
                                  | (Yes)
                        +---------v---------+
                        | staticEval >= beta |----(No)----> [ Bypass NMP ]
                        +---------+---------+
                                  | (Yes)
                        +---------v---------+
                        | hasNonPawnMaterial |----(No)----> [ Bypass NMP (Zugzwang Guard) ]
                        +---------+---------+
                                  | (Yes)
                    [ Execute Transactional Null Move ]
```

### 2.1 Check Avoidance (`!inCheck`)
When the king is in check, passing the turn is illegal under chess rules (exposing the king to capture). NMP is strictly forbidden while in check.

### 2.2 Consecutive Null-Move Prevention (`allowNull`)
If both sides were allowed to pass turns consecutively, the search would enter an artificial cycle. Boson propagates a boolean `allowNull` flag through recursive descent. When executing a null-search child, `allowNull = false` is unconditionally passed, preventing consecutive null moves.

### 2.3 Depth Floor (`depth >= 3`)
With a reduction constant $R = 2$, null moves require $d - 1 - R \ge 0$. At depth $< 3$, the reduced depth falls into quiescence search without sufficient tactical horizon.

### 2.4 Static Evaluation Guard (`staticEval >= beta`)
A null move is only attempted if the static positional evaluation already equals or exceeds $\beta$. If the static evaluation is already below $\beta$, passing a turn will not produce a beta-cutoff, eliminating futile search overhead.

### 2.5 Non-Pawn Material Zugzwang Guard (`pos.hasNonPawnMaterial(side)`)
In pawn endgames (e.g. King + Pawns vs. King + Pawns), zugzwang is prevalent: forcing a player to move may destroy their pawn structure or lose key opposition. If the side to move possesses **only pawns and a king** ($N = B = R = Q = 0$), NMP is strictly disabled:
```cpp
bool Position::hasNonPawnMaterial(Color side) const noexcept {
    const Bitboard n = (side == Color::White) ? m_pieces[WhiteKnight] : m_pieces[BlackKnight];
    const Bitboard b = (side == Color::White) ? m_pieces[WhiteBishop] : m_pieces[BlackBishop];
    const Bitboard r = (side == Color::White) ? m_pieces[WhiteRook]   : m_pieces[BlackRook];
    const Bitboard q = (side == Color::White) ? m_pieces[WhiteQueen]  : m_pieces[BlackQueen];
    return (n | b | r | q) != 0ULL;
}
```
Bypassing NMP when no non-pawn pieces exist protects against catastrophic zugzwang mis-evaluations while maintaining aggressive pruning throughout the opening, middlegame, and piece endgames.

---

## 3. Transactional Null Move & Hash Symmetry

The null move transition is encapsulated within [`Position::makeNullMove`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/src/board/Position.cpp) and [`Position::undoNullMove`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/src/board/Position.cpp) (delegated via [`MoveExecutor`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/src/board/MoveExecutor.cpp)):

```cpp
void Position::makeNullMove(UndoState& undoState) noexcept {
    undoState.castlingRights  = m_castlingRights;
    undoState.enPassantSquare = m_enPassantSquare;
    undoState.halfmoveClock   = m_halfmoveClock;
    undoState.capturedPiece   = Piece::None;
    undoState.movingPiece     = Piece::None;
    undoState.hashKey         = m_hashKey;

    setEnPassantSquare(Square::None); // XORs out enPassant key if present

    m_sideToMove = (m_sideToMove == Color::White) ? Color::Black : Color::White;
    toggleSideHash();                 // XORs Zobrist::s_sideToMove

    m_halfmoveClock++;
    if (m_sideToMove == Color::White) {
        m_fullmoveNumber++;
    }
}
```

### Invariant Proof
1. **Hash Key Synchronization**:
   - Clearing the en-passant square XORs out $Z_{\text{ep}}(S_{\text{ep}})$ if an en-passant target was active.
   - `toggleSideHash()` XORs $Z_{\text{side}}$.
   - Transposition table probes inside the child null-search query the exact, legitimate Zobrist key for the opponent's perspective with no en-passant square.
2. **Reversible Restoration**:
   - `undoNullMove()` reverses `m_fullmoveNumber` (if incremented), flips `m_sideToMove`, XORs $Z_{\text{side}}$ back, restores `m_enPassantSquare` (re-applying $Z_{\text{ep}}$ if needed), and restores `m_halfmoveClock`.
   - Post-condition assertion verifies:
     $$\text{pos.getHashKey()} \equiv \text{undoState.hashKey}$$
     and $P_{\text{restored}} \equiv P_{\text{original}}$ bit-for-bit.

---

## 4. Reduced Null Window Call & Telemetry

The null search executes with a minimal zero-window around $\beta$:
$$\text{nullDepth} = \text{depth} - 1 - R \quad (R = 2)$$
$$\text{nullScore} = -\text{negamax}(\text{pos}, \text{nullDepth}, -\beta, -\beta + 1, \text{ply} + 1, \text{nullPv}, \text{false}, \text{Move}())$$

If $\text{nullScore} \ge \beta$:
- Telemetry: `stats.nullCutoffs++`, `stats.nullMoveCutoffs++`, `stats.betaCutoffs++`.
- The position immediately terminates with $\beta$.

### Telemetry Reporting
[`SearchStatistics`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/include/search/SearchStatistics.hpp) tracks NMP efficacy:
```
--- Null Move Pruning Analytics ---
  -> Null Move Attempts      : 29352
  -> Null Move Cutoffs       : 29320
  -> Null Move Failures      : 32
  -> Zugzwang Protections    : 0
```