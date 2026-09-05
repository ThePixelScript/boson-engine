# Board Architecture Specification

This document defines the architectural specification for the board representation in the Boson chess engine, detailing data structures, memory ownership, state invariants, presentation isolation, and layered validation.

---

## 1. Role of the Position Abstraction

The `Position` class serves as the canonical state of the universe within Boson. It encapsulates the complete mathematical representation of a chess position at a specific instant in time.

- **Single Source of Truth:** All engine subsystems—Move Generation, Negamax Search, Static Exchange Evaluation, Hand-Crafted Evaluation, and UCI Protocol translation—operate exclusively on `Position` instances.
- **Consumer Isolation:** External subsystems receive read-only references (`const Position&`). Subsystems query bitboards, piece distributions, side to move, and castling rights without mutating internal fields or leaking engine state.
- **Self-Sufficiency:** A `Position` instance contains all information required to:
  1. Generate all legal and pseudo-legal moves.
  2. Evaluate positional score and material balance.
  3. Detect terminal draw states (threefold repetition, 50-move rule).
  4. Compute and incrementally maintain 64-bit Zobrist hash keys.
  5. Serialize to and deserialize from standard FEN strings.

---

## 2. Memory Ownership and State Mutability Rules

### Physical Board State vs. Auxiliary Game State
The `Position` memory layout enforces a clear conceptual separation between the spatial physical layout of pieces and auxiliary game rules data:

```
Position Memory Layout
├── Physical Board State (Spatial Representation)
│   ├── std::array<Bitboard, 12> m_pieces    # 12 piece-color 64-bit masks
│   └── std::array<Bitboard, 3>  m_occupancy # White, Black, and Total bitboards
│
└── Auxiliary Game State (Rules & History Engine)
    ├── Color          m_sideToMove         # White (0) or Black (1)
    ├── Square         m_enPassantSquare    # Target en-passant square or Square::None
    ├── CastlingRights m_castlingRights     # Bitmask: WhiteOO, WhiteOOO, BlackOO, BlackOOO
    ├── uint16_t       m_halfmoveClock      # Reversible moves since pawn push or capture
    ├── uint16_t       m_fullmoveNumber     # Incremented after Black's move
    ├── uint64_t       m_hashKey            # Incremental Zobrist hash
    └── Square         m_whiteKingSquare    # Cached King location
        Square         m_blackKingSquare    # Cached King location
```

### Memory Ownership & Allocation Model
- **Pure Value Semantics:** `Position` has value semantics and is fully copyable, movable, and stack-allocated. It contains no heap allocations, no pointer ownership, and no virtual method tables.
- **Thread Safety:** Because each worker thread in future parallel search (Lazy SMP) operates on its own stack-allocated `Position` instance, state modifications require no synchronization primitives or mutex locks.

### The Make/Undo State Machine & Transactional Integrity Model
Tree search lookahead in Boson mutates position state in-place and rolls it back deterministically using a transactional state machine:

1. **State Mutation (`MoveExecutor::makeMove`):** Applies piece displacement, updates occupancy masks, updates clocks, toggles active turn, and maintains incremental Zobrist hashes.
2. **State Rollback (`MoveExecutor::undoMove`):** Inverts piece displacement, restores captured pieces, restores clocks, and restores previous castling and en-passant states.

```
                  makeMove(pos, move, undo)
  Position [Ply N] ─────────────────────────► Position [Ply N+1]
        ▲                                           │
        │                                           │
        └───────────────────────────────────────────┘
                  undoMove(pos, move, undo)
```

### Reversible vs. Irreversible State Mechanics
Chess moves comprise both reversible and irreversible state transformations:

- **Reversible via Move Geometry Alone:**
  - Standard quiet piece displacements: The moving piece is moved from `from` to `to`, and reversed by moving it from `to` back to `from`.
  - Side to move: Toggled between White and Black, and reversed by toggling back.
  - Fullmove number: Incremented after Black's move, decremented when inverting Black's move.

- **Irreversible State Requiring Explicit `UndoState` Snapshots:**
  - **Captured Pieces:** Once captured, the victim piece's type and square cannot be reconstructed from the move object alone (particularly in en-passant where the captured pawn square differs from the move destination square). `UndoState::capturedPiece` preserves this data.
  - **Castling Rights:** Castling availability is permanently stripped when a King or Rook moves, or when a Rook is captured on its home square. Previous rights cannot be re-derived from piece positions alone. `UndoState::castlingRights` caches the exact bitmask.
  - **En Passant Target Square:** An active en-passant square persists for exactly one half-move. If the next move is not an en-passant capture, the target square is cleared. `UndoState::enPassantSquare` preserves the previous square for rollback.
  - **Halfmove Clock:** Resets to 0 upon pawn pushes and captures, but increments on quiet moves. Previous clock values cannot be deduced backwards without caching. `UndoState::halfmoveClock` records the prior value.
  - **Zobrist Key Verification:** `UndoState::hashKey` snapshots the 64-bit Zobrist key prior to move execution, enabling debug assertions to verify exact hash restoration.

### Why `UndoState` is Stack-Allocated on Recursion Frames
Boson rejects heap-backed move history structures (such as `std::vector<UndoState>` stored on the heap):

- **Zero Allocation Overhead:** Dynamic heap allocations during search tree exploration incur malloc/free overhead, thread contention, and heap fragmentation. A stack-allocated `UndoState` (~32 bytes) resides in the CPU L1 data cache on the active thread's call frame.
- **Cache Locality:** When an alpha-beta branch finishes evaluation and returns, the `UndoState` on the current stack frame is immediately available in L1 cache for the `undoMove` operation.
- **Automatic Lifetime Management:** Stack unwinding handles state lifecycles automatically via RAII, eliminating memory leaks or history desynchronization across aspiration window re-searches or search cutoffs.

### Bit-for-Bit Identity Guarantees
Because bitboard modifications and incremental Zobrist updates utilize bitwise XOR operations, all operations are mathematically self-inverting:
$$A \oplus B \oplus B = A$$
Boson strictly guarantees that executing `makeMove` followed immediately by `undoMove` leaves the `Position` struct bit-for-bit identical to its original state:
- All 12 piece bitboards (`m_pieces`) are identical.
- All 3 composite occupancy bitboards (`m_occupancy`) are identical.
- All auxiliary variables (`m_sideToMove`, `m_enPassantSquare`, `m_castlingRights`, `m_halfmoveClock`, `m_fullmoveNumber`, king square caches) are identical.
- The 64-bit Zobrist hash key (`m_hashKey`) is identical.
This invariant is enforced via `assert(pos == originalPos)` and `assert(pos.getHashKey() == originalHash)`.

---

## 3. Prohibition of Duplicate Mutable State

A primary source of subtle, difficult-to-reproduce bugs in chess engines is state desynchronization caused by maintaining duplicate mutable structures (such as maintaining both a 64-bit bitboard and an 8x8 mailbox array during search).

Boson strictly bans duplicate mutable representations:
1. **Zero Dual-Board Overhead:** The board is represented solely by bitboards. There is no parallel mailbox array (`Piece board[64]`) kept during search. When the piece on a square is needed, it is extracted on-demand via bit scanning (`_tzcnt_u64` / bitmask intersection) rather than incrementally synchronized.
2. **Occupancy Invariant:** The composite occupancies (`m_occupancy[White]`, `m_occupancy[Black]`, `m_occupancy[None/Total]`) are mathematical unions of the underlying 12 piece bitboards:
   $$\text{Total Occupancy} = \bigcup_{p \in \text{White}} \text{Bitboard}(p) \cup \bigcup_{p \in \text{Black}} \text{Bitboard}(p)$$
   These composite masks are updated alongside piece modifications, and checked in debug builds via strict invariance assertions.
3. **King Square Cache:** The King squares (`m_whiteKingSquare`, `m_blackKingSquare`) are updated directly inside `setPieceBit` and `clearPieceBit` when a King bit is modified. This is strictly a localized fast lookup for check verification, synchronized directly at the bit manipulation boundary.

---

## 4. Separation of Presentation (BoardPrinter) from State (Position)

A fundamental design boundary in Boson is that the `Position` class is purely a mathematical state model; it has zero knowledge of visual presentation, terminals, or character encodings.

- **Position Must Never Print Itself:** All console output, formatting, and visualization methods are explicitly excluded from `Position`. Embedding I/O operations into domain state creates tight coupling with `std::iostream`, bloats data headers, and creates hidden performance hazards in search hot paths.
- **Stateless Presentation Utility:** The `BoardPrinter` class is a standalone, stateless utility. It consumes `const Position&` and renders output to an arbitrary output stream (`std::ostream& os = std::cout`).
- **Flexible Stream Targets:** Because `BoardPrinter` targets `std::ostream`, presentation output can be directed to `std::cout`, redirected to a log file via `std::ofstream`, or captured for automated assertion testing via `std::stringstream` without modifying engine internals.
- **Dual Rendering Modes:**
  - `Mode::Human`: Standard 8x8 ASCII board grid with ranks 1–8, files a–h, and piece characters (`P`, `N`, `B`, `R`, `Q`, `K`, `p`, `n`, `b`, `r`, `q`, `k`, `.`).
  - `Mode::Debug`: 8x8 ASCII board grid plus complete auxiliary state metadata (active side, castling bitmask, en-passant coordinate, halfmove clock, fullmove counter, and hexadecimal total occupancy mask).

---

## 5. Layered Validation Architecture (Syntax vs. Semantics)

Input validation for FEN strings is strictly decoupled into two distinct architectural layers:

```
Raw FEN String
      │
      ▼
[ Layer 1: Syntactic Validation (FenParser::parse) ]
  ├── Field count check (4-6 whitespace-delimited tokens)
  ├── Rank count verification (exactly 8 ranks separated by '/')
  ├── File count verification (ranks must sum to exactly 8 squares)
  ├── Token character validation (piece symbols, active color 'w'/'b', castling flags)
  └── Numeric clock conversions
      │
      ├─► Success: Instantiates raw Position struct
      └─► Failure: Returns ParseError (InvalidPiecePlacement, MalformedFieldCount, etc.)
            │
            ▼
[ Layer 2: Semantic Validation (FenParser::validateSemantics) ]
  ├── King count invariant (std::popcount(WhiteKing) == 1 && std::popcount(BlackKing) == 1)
  └── Pawn placement invariant (no pawns on Rank 1 [0-7] or Rank 8 [56-63])
      │
      ├─► Success: Returns valid game state
      └─► Failure: Returns ParseError (MissingKing, PawnsOnFirstOrLastRank)
```

### Rationale for Decoupling
1. **Utility of Syntactically Valid Non-Game States:** Low-level algorithmic testing frequently requires custom or partial board states:
   - An empty board (`8/8/8/8/8/8/8/8 w - - 0 1`) is syntactically valid and essential for testing piece leaper attacks, ray generation, and isolated piece mobility without dummy kings interfering.
   - Positional puzzle fragments or endgame study setups may isolate subtrees that do not satisfy full game invariants during intermediate test stages.
2. **Fail-Fast Error Attribution:** Separating syntax from semantics enables precise diagnostics. A malformed string with 9 files is rejected at parse time with `ParseError::InvalidPiecePlacement`, whereas a structurally valid string missing a king is identified with `ParseError::MissingKing`.
3. **Strict Validation Pipeline (`FenParser::parseStrict`):** For live game engines and UCI frontends where only fully legal chess positions are acceptable, `FenParser::parseStrict` chains syntactic parsing and semantic validation into a single call.

---

## 6. Independent Subsystem Testability

The decoupling of `Position`, `FenParser`, and `BoardPrinter` ensures each component can be verified in isolation:

- **No Engine Dependencies:** `Position`, `FenParser`, and `BoardPrinter` do not include or depend on `MoveGenerator`, `Search`, `Evaluator`, `TimeManager`, or UCI protocol handlers.
- **Micro-Benchmarking & Fuzzing:** The FEN parser can be independently fuzzed with millions of malformed strings without risking search state corruption or initializing large transposition tables.
- **Fast Deterministic Test Suites:** Unit tests in `tests/TestRunner.cpp` execute comprehensive syntax and semantic checks in milliseconds, ensuring continuous verification without requiring search tree traversal.

---

## 7. Extension Points for Evaluation and NNUE Accumulators

The board representation is designed with explicit extension points to support future optimization milestones without changing public APIs:

### Hand-Crafted Evaluation (HCE) Hooks
The 12 piece bitboards and composite occupancies provide direct primitives for evaluation:
- Fast bit population counts (`std::popcount`) directly index piece values for material calculation.
- Piece-Square Table (PST) scoring iterates active bits using bitscan forward operations (`_BitScanForward64` or `_tzcnt_u64`), mapping squares directly to evaluation weight matrices.
- Pawn structure terms (passed, isolated, doubled, backward) operate directly on column and rank bitboard operations (`Bitboard` shifts and file masks).

### Future NNUE Accumulator Extensions
When implementing Efficiently Updatable Neural Networks (NNUE, Milestone 8):
- **HalfKP / HalfKAv2 Feature Mapping:** Neural network features represent pairs of `(KingSquare, PieceSquare)`.
- **Differential Accumulator Updates:** The `MoveExecutor` transactional boundary provides the exact injection point for updating neural accumulators:
  - Add feature: `(FriendlyKing, piece, toSquare)`
  - Sub feature: `(FriendlyKing, piece, fromSquare)`
  - If capture: Sub feature `(FriendlyKing, capturedPiece, toSquare)`
- Because `UndoState` already caches the captured piece and previous King positions, NNUE accumulator stack unwinding maps directly onto the existing transactional `undoMove` architecture with zero changes to `Position`'s external interface.

### Sliding Piece Generation Upgrades
The `MoveGenerator` currently uses precalculated sliding ray tables. The internal implementation can be swapped to hardware BMI2 PEXT lookups (`_pext_u64`) or Magic Bitboards entirely inside `MoveGenerator.cpp` and `Bitboard.hpp`, maintaining full backward compatibility with the `Position` class.