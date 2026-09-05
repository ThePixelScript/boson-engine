# Boson Move System Architecture & Invariants Specification

## 1. Architectural Overview & Design Rationale

The move execution subsystem in Boson (`MoveExecutor`) implements a transactional state machine operating on the `Position` representation. Search algorithms (such as alpha-beta minimax with iterative deepening and quiescence search) traverse millions of tree nodes per second. To achieve high throughput, cloning full board states on the heap or copy-constructing large state objects per tree edge is prohibited.

Instead, Boson enforces a transactional mutation and inversion lifecycle:
$$\text{Position}_{t} \xrightarrow{\text{makeMove}(M, S)} \text{Position}_{t+1} \xrightarrow{\text{undoMove}(M, S)} \text{Position}_{t}$$

The state of the universe after move application followed by inverse application must be bit-for-bit identical to its pre-move state:
$$\text{undoMove}(\text{makeMove}(P, M, S), M, S) \equiv P$$

This identity covers piece bitboards, occupancy layers, side to move, castling rights bitmasks, en passant target squares, halfmove clocks, fullmove counters, cached king squares, and incremental 64-bit Zobrist hash keys.

---

## 2. Move Lifecycle: Scalar Value Semantics

The `Move` class (`engine/include/board/Move.hpp`) is designed as an immutable, trivially copyable 32-bit scalar value (`sizeof(Move) == 4`). It encapsulates the complete geometric and mechanical description of a ply without external dependencies or heap allocations.

### Bitfield Encoding Layout

The 32-bit internal integer (`m_data`) packs the following contiguous fields into 18 active bits:

| Bits | Field | Type | Range / Description |
| :--- | :--- | :--- | :--- |
| `00..05` (6 bits) | `from` | `Square` | Origin square index $[0 \dots 63]$ |
| `06..11` (6 bits) | `to` | `Square` | Destination square index $[0 \dots 63]$ |
| `12..14` (3 bits) | `flags` | `Move::Flags` | Special move categorization |
| `15..17` (3 bits) | `promo` | `Move::PromotionPiece` | Promotion piece classification |
| `18..31` (14 bits) | Reserved | `uint32_t` | Zero-initialized padding for alignment and future metadata |

### Flag Definitions

- `Flags::None` (`0`): Normal quiet move or standard capture.
- `Flags::DoublePawnPush` (`1`): Two-square pawn advance establishing an en passant target square.
- `Flags::Castling` (`2`): King move triggering simultaneous relocation of the home rook.
- `Flags::EnPassant` (`3`): Pawn diagonal capture into empty space removing the adjacent enemy pawn.
- `Flags::Promotion` (`4`): Pawn reaching the 8th rank transforming into a Queen, Rook, Bishop, or Knight.

`Move` instances are passed by value across search plies, move ordering arrays, transposition tables, and move generation buffers with zero allocation overhead.

---

## 3. The Canonical 7-Step Mutation Sequence (`makeMove`)

To ensure deterministic state transitions and prevent bitboard desynchronization, `MoveExecutor::makeMove` follows an invariant 7-step sequence:

```
[1. Save UndoState]
        │
        ▼
[2. Remove Moving Piece from 'from']
        │
        ▼
[3. Remove Captured Piece (to / ep square)]
        │
        ▼
[4. Place Moving / Promoted Piece on 'to']
        │
        ▼
[5. Update Occupancies & King Caches]
        │
        ▼
[6. Update Game State & Clocks]
        │
        ▼
[7. Switch Side to Move & Toggle Turn Hash]
```

### Step Breakdown

1. **Save UndoState**:
   Capture the pre-move irreversible state into caller-allocated stack storage:
   - Current castling rights bitmask.
   - Current en passant square.
   - Halfmove reversible clock.
   - Pre-move Zobrist 64-bit hash key.
   - Initialize `capturedPiece = Piece::None`.
   - Initialize castling rook coordinates to `Square::None`.

2. **Remove Moving Piece**:
   Extract the moving piece type from the origin square `from`. Record `movingPiece` in `UndoState`. Clear the piece bit from `m_pieces[movingPiece]` via `clearPieceBit()`, which incrementally updates the piece hash.

3. **Remove Captured Piece**:
   - **En Passant**: Compute the victim square ($to - 8$ for White, $to + 8$ for Black). Clear the enemy pawn bitboard and hash at the victim square. Record `capturedPiece` as the opposing pawn.
   - **Standard Capture**: Inspect all 12 piece bitboards at `to`. If a bit is set, record the captured piece in `UndoState` and clear its bitboard and hash at `to`.
   - **Quiet Move**: No capture occurs; `capturedPiece` remains `Piece::None`.

4. **Place Moving Piece on Destination**:
   - **Promotion**: Determine the promoted piece type (`WhiteQueen`, `WhiteRook`, etc.) and set its bitboard and hash at `to` via `setPieceBit()`.
   - **Standard Move**: Set `movingPiece` on `to` via `setPieceBit()`.
   - **Castling**: Relocate the corresponding rook: clear the rook from its home corner (`H1`, `A1`, `H8`, `A8`) and set it on its post-castling square (`F1`, `D1`, `F8`, `D8`), recording rook origin and destination in `UndoState`.

5. **Update Occupancies & King Square Caches**:
   Execute `pos.updateOccupancy()`. Reconstruct `m_occupancy[White]` and `m_occupancy[Black]` via bitwise OR across piece arrays, update `m_occupancy[None]` (total occupancy), and synchronize cached king squares (`m_whiteKingSquare`, `m_blackKingSquare`).

6. **Update Game State & Clocks**:
   - **Castling Rights**: Strip rights if the king moved or if either rook moved from / was captured on its home square.
   - **En Passant Square**: If `Flags::DoublePawnPush`, assign the skipped middle square as active en passant target; otherwise clear to `Square::None`.
   - **Halfmove Clock**: Reset to `0` on pawn moves or captures; increment by `1` on quiet non-pawn moves.
   - **Fullmove Number**: Increment when Black completes a move.

7. **Switch Side to Move**:
   Toggle `m_sideToMove` between `Color::White` and `Color::Black`. Apply `toggleSideHash()` to update the Zobrist turn key.

8. **Debug Invariant Assertion**:
   Enforce structural invariants in debug builds (`assert(...)`).

---

## 4. The Undo Lifecycle: Reversion & Inversion Symmetry (`undoMove`)

`MoveExecutor::undoMove` inverts the mutation sequence in exact reverse causal order:

1. **Revert Active Side & Fullmove Number**:
   - Decrement `m_fullmoveNumber` if the original moving player was Black.
   - Reset `m_sideToMove` to original side and toggle side hash.

2. **Revert Piece Placement**:
   - If castling, return the rook from `castlingRookTo` to `castlingRookFrom`.
   - If promotion, clear the promoted piece from `to`; otherwise clear `movingPiece` from `to`.
   - Set `movingPiece` back onto origin square `from`.

3. **Revert Captured Piece**:
   - If en passant, restore the captured pawn at the calculated victim square.
   - If a standard capture occurred (`capturedPiece != Piece::None`), restore the captured piece at `to`.

4. **Restore Game State & Clocks**:
   - Restore castling rights bitmask from `undoState.castlingRights`.
   - Restore en passant target square from `undoState.enPassantSquare`.
   - Restore halfmove clock from `undoState.halfmoveClock`.

5. **Reconstruct Occupancies & King Caches**:
   Execute `pos.updateOccupancy()` to recompute white, black, and all-piece masks and synchronize king squares.

6. **Verify Identity & Invariants**:
   Under debug configurations, assert that `pos.getHashKey() == undoState.hashKey` and confirm bitboard invariants.

---

## 5. Position Invariants & Structural Guarantees

Every valid `Position` state before and after any move operation must satisfy five structural invariants:

### Invariant 1: Total Occupancy Consistency
$$\text{TotalOccupancy} = \text{ColorOccupancy}(\text{White}) \cup \text{ColorOccupancy}(\text{Black})$$
No square may be marked occupied in the combined occupancy bitboard unless a white or black piece is present.

### Invariant 2: Color Disjointness (No Piece Overlap)
$$\text{ColorOccupancy}(\text{White}) \cap \text{ColorOccupancy}(\text{Black}) = \emptyset \quad (0\text{ULL})$$
White and Black pieces must never occupy the same square simultaneously.

### Invariant 3: Piece Partition Invariance
For each color $C \in \{\text{White}, \text{Black}\}$:
$$\text{ColorOccupancy}(C) = \bigcup_{p \in \text{Pieces}(C)} \text{PieceBitboard}(p)$$
$$\forall p_i, p_j \in \text{Pieces}(C) \quad (i \neq j) \implies \text{PieceBitboard}(p_i) \cap \text{PieceBitboard}(p_j) = \emptyset$$
Individual piece bitboards partition their respective color occupancy with zero bit collisions.

### Invariant 4: King Square Cache Coherence
$$\forall C \in \{\text{White}, \text{Black}\}: \quad \text{popcount}(\text{PieceBitboard}(\text{King}_C)) = 1 \implies 1\text{ULL} \ll \text{KingSquare}(C) \equiv \text{PieceBitboard}(\text{King}_C)$$
The cached king coordinate must match the single active bit of that color's king bitboard.

### Invariant 5: Reversible Hash Key Symmetry
$$\forall P, M: \quad \text{hash}(\text{undoMove}(\text{makeMove}(P, M, S), M, S)) \equiv \text{hash}(P)$$
Incremental Zobrist XOR operations are strictly self-inverting ($A \oplus B \oplus B = A$).

---

## 6. `UndoState` Design & Memory Model

`UndoState` (`engine/include/board/UndoState.hpp`) provides the exact delta needed to reverse irreversible transitions without storing full board duplicates.

### Struct Definition & Memory Layout

```cpp
struct UndoState {
    CastlingRights castlingRights;           // 1 byte
    Square         enPassantSquare;          // 1 byte
    int            halfmoveClock;            // 4 bytes
    Piece          capturedPiece;            // 1 byte
    Piece          movingPiece;              // 1 byte
    Square         castlingRookFrom;         // 1 byte
    Square         castlingRookTo;           // 1 byte
    Piece          castlingRookPiece;        // 1 byte
    // 3 bytes compiler padding for 64-bit alignment
    uint64_t       hashKey{0ULL};            // 8 bytes
};
```

### Allocation & Lifecycle Invariants

- **Storage Class**: Allocated exclusively on the execution call stack by the caller (typically as a local stack variable in recursive search functions `searchAlphaBeta` or `searchQuiescence`).
- **Memory Footprint**: `sizeof(UndoState) <= 32` bytes (24 bytes packed/aligned).
- **Trivial Lifecycle**: `std::is_trivially_destructible_v<UndoState>` is `true`. No destructors, no virtual tables, and no dynamic memory allocations (`new` / `malloc`).
- **Zero Cache Thrashing**: Stack allocation ensures cache locality within L1 data cache across deep alpha-beta search traversals.

---

## 7. Extension Points

The move subsystem is architected to interface cleanly with downstream engine components:

1. **Move Ordering & Scoring**:
   - `Move` fits inside 32-bit registers, allowing paired packaging:
     ```cpp
     struct ScoredMove {
         Move move;
         int32_t score;
     };
     ```
   - 64-bit sorting keys can pack score (32 bits) and raw move data (32 bits) for high-speed SIMD or scalar register sorting without indirection.

2. **Check Detection & Fast King Attacks**:
   - Cached king squares (`getKingSquare(Color)`) allow $O(1)$ attack lookups using magic bitboards without scanning the board.
   - `MoveExecutor` provides the baseline state transitions for in-check validation after candidate moves.

3. **NNUE Feature Accumulator Updates**:
   - The 7-step sequence isolates exact removal and addition events:
     - `SubPiece(from, movingPiece)`
     - `SubPiece(to / victimSq, capturedPiece)` (if capture)
     - `AddPiece(to, placedPiece)`
     - `SubPiece(rookFrom)` & `AddPiece(rookTo)` (if castling)
   - These discrete events map directly to half-kp / half-ka NNUE accumulator additions and subtractions, enabling incremental accumulator maintenance during `makeMove` and lazy reversal during `undoMove`.