# Boson Move Generation & Perft Verification Architecture

## 1. Architectural Boundaries & Design Rationale

The `MoveGenerator` subsystem (`engine/include/board/MoveGenerator.hpp`, `engine/src/board/MoveGenerator.cpp`) is designed as a side-effect-free, purely functional inspection service operating over the `Position` representation. It inspects board state and populates a high-performance, stack-allocated `MoveList` container:

$$\text{Position (const\&)} \longrightarrow \text{MoveGenerator} \longrightarrow \text{MoveList}$$

### Read-Only Invariance (`const Position&`)

Move generation is fundamentally an analytical operation, not a mutative one. Mutating game state during generation introduces severe architectural hazards:
- **Synchronization Hazards**: If an exception or early return interrupts move generation while state is mutated, the universe is permanently corrupted.
- **Cache Pollution**: In-place modification of position bitboards during generation dirties L1/L2 cache lines unnecessarily.
- **API Misuse**: A caller passing a `const Position&` expects mathematical immutability. Enforcing `const Position&` across 100% of `MoveGenerator`'s public and internal APIs guarantees that caller state is never modified.

When `generateLegalMoves` validates king exposure via make/undo transitions, it operates strictly on a local stack copy (`Position tempPos = pos;`). The caller's `pos` remains bit-for-bit invariant.

---

## 2. The 3-Layer Architecture

Move generation is structured into three discrete, decoupled layers separating raw geometry, move creation, and rule-based legality filtering.

```
┌────────────────────────────────────────────────────────┐
│ Layer 1: Attack Generation (Pure Geometry & Lookups)   │
│ - Precalculated tables: Knights, Kings                 │
│ - Directional Ray Casts: Rooks, Bishops, Queens        │
│ - Shift masks: Pawn single/double & captures           │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Layer 2: Pseudo-Legal Move Generation                  │
│ - Target masks (friendly vs enemy occupancy)           │
│ - Pawn mechanics (single/double push, ep, promotions)  │
│ - Castling path occupancy clearance                    │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Layer 3: Legal Move Filtering                          │
│ - In-check home square verification                    │
│ - Castling transit attack checks (E1-F1-G1, etc.)      │
│ - Transactional make/undo king safety validation       │
└────────────────────────────────────────────────────────┘
```

### Layer 1: Attack Generation

Layer 1 computes pure bitboard attack geometries without concern for friendly pieces, king safety, or chess rules:

1. **Leaping Attack Tables (Knights & Kings)**:
   - Initialized at engine boot via `MoveGenerator::initializeTables()`.
   - `s_knightAttacks[64]`: 64 precalculated bitboards handling compass jumps ($\pm 17, \pm 15, \pm 10, \pm 6$) with A/B and G/H file boundary masks.
   - `s_kingAttacks[64]`: 64 precalculated bitboards handling 8-neighborhood king steps.
   - Retrieval is $O(1)$ scalar array indexing: `s_knightAttacks[sq]`.

2. **Sliding Ray Trajectories (Rooks, Bishops, Queens)**:
   - Precalculated unblocked directional ray bitboards: `s_rookRays[64][4]` (N, S, E, W) and `s_bishopRays[64][4]` (NE, NW, SE, SW).
   - Attack computation intersects rays with board occupancy to isolate blocker bitboards:
     - `_BitScanForward64` (LSB) isolates the closest blocker for positive ray directions.
     - `_BitScanReverse64` (MSB) isolates the closest blocker for negative ray directions.
     - Ray bits beyond the first blocker are masked out using precalculated ray tails: `ray &= ~tail`.
   - Queen attacks are the bitwise OR of rook and bishop attacks: `getRookAttacks(sq, occ) | getBishopAttacks(sq, occ)`.

3. **Pawn Geometric Attacks**:
   - Single-step shifts with file wrapping guards:
     - White: `(pawn & ~A_FILE) << 7 | (pawn & ~H_FILE) << 9`
     - Black: `(pawn & ~A_FILE) >> 9 | (pawn & ~H_FILE) >> 7`

### Layer 2: Pseudo-Legal Move Generation

Layer 2 translates raw geometric rays and attack tables into concrete pseudo-legal `Move` instances by applying the **Geometry $\to$ Occupancy $\to$ Ownership** pipeline philosophy:

1. **The Geometry $\to$ Occupancy $\to$ Ownership Pipeline Philosophy**:
   - **Geometry (Layer 1)**: Defines unconstrained physical capabilities (e.g. king 8-neighborhood leaps, directional pawn shifts $\pm 8$, diagonal capture vectors $\pm 7, \pm 9$).
   - **Occupancy Filter**: Intersects geometric paths with physical matter (`totalOccupancy`). For sliding pieces, occupancy halts ray projection at the first blocker; for pawns, push paths require empty occupancy (`~totalOccupancy`).
   - **Ownership Filter**: Partitions landing squares based on piece color:
     $$\text{ValidTargets} = \text{Attacks} \cap \sim\text{FriendlyOccupancy}$$
     Squares containing friendly pieces are pruned; squares containing enemy pieces or empty squares constitute valid pseudo-legal moves.

2. **Phase B: King Move Generation (`generateKingMoves`)**:
   - **Precalculated Lookups**: `s_kingAttacks[sq]` provides the 8-directional compass mask around the king in $O(1)$.
   - **Ownership Filtering**: `validMoves = s_kingAttacks[sq] & ~friendlyOccupancy`. This naturally includes empty squares (quiet moves) and squares occupied by opponent pieces (captures).
   - **Pure Pseudo-Legality**: King moves are generated strictly without regard to whether the destination square is attacked or whether the king is currently in check. Check detection and king exposure are deferred to Layer 3 legal filtering.
   - **Castling Generation**: Emits pseudo-legal `Move::Flags::Castling` moves solely on the basis of active `CastlingRights` and intermediate square clearance in `totalOccupancy` (F1/G1, D1/C1/B1 for White; F8/G8, D8/C8/B8 for Black). Attack transit validation (E1, F1, G1 not attacked) is deferred entirely to Layer 3.

3. **Phase C: Modular Pawn Move Generation (`generatePawnMoves`)**:
   Pawn mechanics are factored into modular subroutines parameterized by move direction (+8 for White, -8 for Black) and starting rank mask (`0xFF00ULL` Rank 2 for White, `0xFF000000000000ULL` Rank 7 for Black):

   - **Phase C1: Single Push (`generatePawnPushes`)**:
     * $target = sq + direction$.
     * Target must be vacant: `!(totalOccupancy & (1ULL << target))`.
     * Promotion check: If target rank is 8 or 1 ($target \ge 56 \lor target \le 7$), emits 4 distinct promotion moves (`Queen`, `Rook`, `Bishop`, `Knight`). Otherwise emits a standard quiet push.
   - **Phase C2: Double Push (`generatePawnPushes`)**:
     * Guarded by starting rank presence: `(1ULL << sq) & startRankMask`.
     * Precondition: Single-push target square was already verified empty in Phase C1.
     * $doubleTarget = target + direction$.
     * Destination must be vacant: `!(totalOccupancy & (1ULL << doubleTarget))`.
     * Emits `Move(sq, doubleTarget, Move::Flags::DoublePawnPush)`.
   - **Phase C3: Diagonal Captures & En Passant (`generatePawnCaptures`)**:
     * Geometry: Left/right diagonal shifts with file wrap guards (`~FileA` and `~FileH`).
     * Target mask: `targetMask = enemyOccupancy | (epSquare != Square::None ? getSquareBit(epSquare) : 0ULL)`.
     * Intersects diagonal attacks with `targetMask`.
     * If target matches `epSquare`, flags `Move::Flags::EnPassant`.
     * If target lands on rank 8 or 1, emits 4 promotion captures (`Queen`, `Rook`, `Bishop`, `Knight`).
     * Otherwise emits a standard capture.

### Layer 3: Legal Move Filtering

Layer 3 filters out pseudo-legal moves that violate the fundamental law of chess: *the moving player's king must not be left in check*.

1. **Castling Rule Compliance**:
   - The king cannot castle while currently in check (`inCheck(pos, us)`).
   - The king cannot pass through or land on an attacked square:
     - White O-O: `E1`, `F1`, `G1` must not be attacked by Black.
     - White O-O-O: `E1`, `D1`, `C1` must not be attacked by Black (`B1` only needs to be empty, not attack-free).
     - Black O-O: `E8`, `F8`, `G8` must not be attacked by White.
     - Black O-O-O: `E8`, `D8`, `C8` must not be attacked by White (`B8` only needs to be empty, not attack-free).

2. **King Safety Validation via Make/Undo**:
   - For every pseudo-legal move, executes `MoveExecutor::makeMove(tempPos, move, undo)`.
   - Tests `inCheck(tempPos, us)`.
   - If the king is safe (`!inCheck`), pushes `move` into `legalMoves`.
   - Reverts state via `MoveExecutor::undoMove(tempPos, move, undo)`.
   - Correctly handles pinned pieces, discovered checks, en passant horizontal pin exposures, and king retreat paths.

---

## 3. `MoveList` Container Architecture

Search and perft routines generate dozens of move lists per search branch. Using heap-allocating containers such as `std::vector<Move>` in hot paths triggers heap fragmentation and malloc/free overhead.

`MoveList` (`engine/include/board/MoveList.hpp`) is designed as a zero-allocation, fixed-capacity stack container:

```cpp
class MoveList {
public:
    constexpr MoveList() noexcept : m_count(0) {}

    constexpr void push_back(const Move& move) noexcept {
        if (m_count < m_storage.size()) {
            m_storage[m_count++] = move;
        }
    }

    constexpr Move& operator[](size_t index) noexcept { return m_storage[index]; }
    constexpr const Move& operator[](size_t index) const noexcept { return m_storage[index]; }

    constexpr size_t size() const noexcept { return m_count; }
    constexpr bool empty() const noexcept { return m_count == 0; }
    constexpr size_t capacity() const noexcept { return m_storage.size(); }
    constexpr void clear() noexcept { m_count = 0; }

    constexpr auto begin() noexcept { return m_storage.begin(); }
    constexpr auto end() noexcept { return m_storage.begin() + m_count; }
    constexpr auto begin() const noexcept { return m_storage.cbegin(); }
    constexpr auto end() const noexcept { return m_storage.cbegin() + m_count; }

private:
    std::array<Move, 256> m_storage;
    size_t m_count;
};
```

### Memory & Cache Properties

- **Capacity**: 256 moves. The theoretical maximum number of legal moves in any reachable chess position is 218; 256 guarantees safe headroom without overflow.
- **Footprint**: $256 \times 4\text{ bytes} + 8\text{ bytes} = 1,032\text{ bytes}$ (~1 KB). Fits entirely within L1 data cache (typical 32 KB - 48 KB per core).
- **Trivial Destruction**: `std::is_trivially_destructible_v<MoveList>` is `true`. Stack pop releases memory in 0 cycles.

---

## 4. Perft Verification Methodology

Perft (Performance Test) is the universal debugging tool for chess engine move generators. It counts the number of leaf nodes at depth $d$:

$$\text{Perft}(P, d) = \begin{cases} 1 & \text{if } d = 0 \\ \sum_{m \in \text{Legal}(P)} \text{Perft}(\text{make}(P, m), d - 1) & \text{if } d > 0 \end{cases}$$

### Standard Test Suite Reference Matrix

| Position | FEN | Depth | Expected Nodes | Verification Role |
| :--- | :--- | :---: | :---: | :--- |
| **Position 1** (Startpos) | `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1` | 1<br>2<br>3<br>4<br>5 | 20<br>400<br>8,902<br>197,281<br>4,865,609 | Baseline move generation, symmetry, opening pawn pushes. |
| **Position 2** (KiwiPete) | `r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1` | 1<br>2<br>3<br>4 | 48<br>2,039<br>97,862<br>4,085,603 | Extreme tactical complexity, multi-piece pins, castling rights with attacks, promotions, en passant. |
| **Position 3** | `8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1` | 1<br>2<br>3<br>4 | 14<br>191<br>2,812<br>43,238 | Endgame pawn play, rook pins along ranks/files, discovered checks. |
| **Position 4** | `r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1` | 1<br>2<br>3<br>4 | 6<br>264<br>9,467<br>422,333 | Mutual castling rights under heavy pressure, double checks, pawn underpromotions. |

A single bug in castling path clearance, en passant resolution, pin detection, or promotion expansion produces a discrepancy in leaf node counts. Exact leaf node identity across all depths mathematically guarantees engine move generator correctness.

---

## 5. Future Optimization Roadmap

While current ray-cast generation is fully verified and correct, future milestones will introduce performance enhancements:

1. **Magic Bitboards (Fancy / Plain)**:
   - Replace ray-casting loops with $O(1)$ precalculated lookup tables indexed by hashed occupancy masks: `s_rookAttacks[sq][(occupancy & mask) * magic >> shift]`.
   - Eliminates branching in sliding piece generation.

2. **BMI2 `PEXT` Bitboards**:
   - On x86-64 processors supporting BMI2 (`_pext_u64`), replaces magic multiplication with hardware parallel bit extract instructions for ultra-compact attack tables.

3. **In-Check Evasion Specialization**:
   - When `inCheck(pos, us) == true`, bypass standard pseudo-legal generation entirely.
   - Generate only moves that resolve the check: king escapes, capturers of the checking piece, or interpositions along the checking ray.

4. **Staged Move Generation & Picking**:
   - During alpha-beta search, generate moves on-demand in priority stages:
     1. Hash move from Transposition Table.
     2. Winning / equal tactical captures (MVV-LVA / SEE $\ge 0$).
     3. Killer moves.
     4. Counter moves / quiet history moves.
     5. Losing captures (SEE $< 0$).
   - Beta cutoffs occurring in early stages avoid generating the remaining moves entirely, yielding substantial search speedups.