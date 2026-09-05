# Engine Architecture and Subsystem Boundaries

This document defines the structural layout, component boundaries, and interface contracts of the Boson chess engine.

---

## 1. Architectural Philosophy

Boson is organized into four distinct layers. Dependencies flow strictly downstream:

```
[ Application Layer (UCI) ]
            │
            ▼
[ Search & Orchestration ]
            │
            ▼
[ Evaluation Subsystem ]
            │
            ▼
[ Board & Move Core ]
```

### Downstream Dependency Rule
- A higher layer may depend on and consume lower-layer interfaces.
- A lower layer must never include, reference, or callback into a higher layer.
- Subsystems within each layer interact only through declared public APIs; internal representations and cache structures remain encapsulated.

---

## 2. Layer Specifications

### Layer 1: Application Layer (UCI Protocol)
- **Directory:** `engine/src/main.cpp`
- **Responsibility:** Implements the Universal Chess Interface (UCI) protocol. Translates standard input streams into engine commands (`uci`, `isready`, `ucinewgame`, `position`, `go`, `stop`, `quit`).
- **Isolation:** The UCI layer interacts with the engine solely through `Position` initialization (via `FenParser`) and `SearchController`. It has no direct access to bitboard structures, move ordering logic, or evaluation tables.
- **Asynchrony:** Spawns asynchronous search workers and monitors search interruption tokens (`std::atomic<bool>`) to handle GUI `stop` and `quit` signals immediately.

### Layer 2: Search & Orchestration Layer
- **Directory:** `engine/include/search/`, `engine/src/search/`
- **Components:**
  - `Search`: Lookahead driver implementing Alpha-Beta Negamax, Iterative Deepening, and Aspiration Windows.
  - `SearchController`: Asynchronous search lifecycle manager enforcing clock constraints.
  - `TimeManager` & `SearchLimits`: Dynamic budget allocation (soft and hard time thresholds based on remaining time and increments).
  - `MoveOrderer`: Stage-based move sorting coordinating hash moves, MVV-LVA captures, SEE thresholds, Killer moves, Counter-Move History (CMH), 2-Ply Continuation History, and Global History.
  - `TranspositionTable`: 64-bit cluster hash table with depth-preferred replacement and age tracking.
  - `SEE`: Static Exchange Evaluation recursive lookahead simulator for capture safety.
- **Isolation:** Coordinates move generation and calls the evaluation interface. Agnostic to the internal mathematics of the evaluation function (HCE vs. NNUE).

### Layer 3: Evaluation Subsystem
- **Directory:** `engine/include/evaluation/`, `engine/src/evaluation/`
- **Components:**
  - `Evaluator`: Evaluates a given `Position` and returns a centipawn score from the perspective of the side to move.
  - `PieceSquareTables`: Tapered positional tables balancing opening, middlegame, and endgame piece placements.
  - `CorrectionHistoryTable`: Self-calibrating feedback layer adjusting static evaluation scores based on search search-tree cutoff results.
- **Replaceability Contract:** Any evaluation engine satisfying the scoring contract `int evaluate(const Position& pos)` can be substituted without altering search heuristics or board representations.

### Layer 4: Board & Move Core
- **Directory:** `engine/include/board/`, `engine/src/board/`, `engine/include/fen/`, `engine/src/fen/`
- **Components:**
  - Domain Types: Strongly typed `Square`, `Color`, `Piece`, `CastlingRights`, and `Move`.
  - `Bitboard`: 64-bit integer masks and bitwise utility operations (`std::popcount`, `std::countr_zero`, bitboard shifts).
  - `Position`: State holder owning 12 piece bitboards, 3 occupancy bitboards (White, Black, Combined), castling flags, en passant square, clocks, and incremental Zobrist hash keys.
  - `MoveExecutor`: Transactional state machine implementing `makeMove` and `undoMove` using `UndoState` records.
  - `MoveGenerator`: Legal and pseudo-legal move generators, sliding ray tables, knight/king leaper masks, and attack inspectors (`isSquareAttacked`, `inCheck`).
  - `FenParser` & `BoardPrinter`: FEN serialization and visual ASCII/Unicode board diagnostic utilities.

---

## 3. Transactional State Management

Boson uses a transactional model for tree search state mutation:

```
Position (State N)
       │
       ├─ makeMove(Move, UndoState) ──► Position (State N+1)
       │                                       │
       └◄─ undoMove(Move, UndoState) ──────────┘
```

1. **Deterministic Rollback:** `MoveExecutor::makeMove` captures reversible state (previous en passant square, castling rights, halfmove clock, captured piece, and prior hash key) into an `UndoState` struct on the call stack.
2. **Zero Heap Allocation:** `UndoState` instances live strictly on the search recursion stack. No dynamic memory allocation occurs during state transitions.
3. **Synchronization Invariants:**
   - Primary bitboards must sum exactly to the composite occupancy bitboards (`m_occupancy`).
   - Zobrist hash updates must remain strictly symmetrical: hashing a piece out during `makeMove` and in during `undoMove` must restore the exact initial 64-bit key.

---

## 4. Replaceability and Extension Guidelines

- **Evaluation Swapping:** The evaluation layer will support dual-evaluator operation in Milestone 8 (HCE and NNUE). The search engine communicates with evaluation exclusively through position scoring calls, ensuring zero coupling with neural network inference details.
- **Move Generation Upgrades:** The move generator currently utilizes precalculated sliding ray tables. Upgrading to hardware BMI2 PEXT lookups or Magic Bitboards will occur entirely within `MoveGenerator.cpp` and `Bitboard.hpp`, keeping public signatures unchanged.
- **Thread Scaling:** To support multi-threaded search (Lazy SMP), the Transposition Table uses cache-aligned bucket structures. Worker threads will operate on independent stack-allocated `Position` instances, eliminating state synchronization locks in the hot path.