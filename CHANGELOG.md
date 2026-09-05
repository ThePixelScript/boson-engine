# Changelog

All notable changes to the Boson Chess Engine project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Milestone 7: Advanced Positional Evaluation (Tapered HCE, Pawn Structures, Mobility, King Safety, Texel Tuner).
- Milestone 8: Neural Network Evaluation (NNUE HalfKP/HalfKAv2 Inference Engine, SIMD Vectorization).
- Milestone 9: Multithreading & SMP Parallel Search (Lazy SMP, Lockless Shared TT).
- Milestone 10: Endgame Tablebases & Dynamic Time Allocation (Syzygy 3-4-5-6 probing).
- Milestone Ω: Autonomous Self-Play Framework & Research Platform.

---

## [0.7.0] - 2026-09-04

### Added
- **Correction History Table**: Dynamic evaluation bias compensation layer tracking search outcome discrepancies.
- **2-Ply Continuation History Matrix**: Move ordering tracking move success in context of preceding piece moves.
- **Counter-Move History (CMH) Table**: L1-cache friendly matrix indexing refutation replies to opponent moves.
- **Late Move Reductions (LMR)**: Logarithmic reduction table indexed by depth and move order with null-window scout re-searches.
- **Static Exchange Evaluation (SEE)**: Standalone recursive swap simulator to classify winning and losing captures.
- **Null Move Pruning (NMP)**: Dynamic $R=2$ pruning with material zugzwang guards.
- **Aspiration Windows**: Narrow scout window framework ($\pm 30$ cp) with exponential window widening upon fail-high or fail-low.

### Changed
- Refactored `MoveOrderer` to integrate TT moves, MVV-LVA, SEE thresholds, Killers, CMH, Continuation History, and Global History.
- Hardened Transposition Table replacement strategy with age and depth preservation guards.

---

## [0.6.0] - 2026-07-28

### Added
- **Quiescence Search**: Tactical tree extension evaluating only captures and promotions to resolve the horizon effect.
- **Stand-Pat Pruning**: Immediate delta-cutoff evaluations within quiescence search.
- **Move Ordering Foundation**: MVV-LVA (Most Valuable Victim - Least Valuable Attacker) capture scoring.
- **Killer Move Heuristic**: 2-slot per ply table for quiet refutation sorting.
- **Global History Heuristic**: Butterfly board matrix tracking quiet move beta-cutoff frequency.

---

## [0.5.0] - 2026-07-15

### Added
- **Zobrist Hashing**: Full 64-bit incremental pseudorandom hash generator for pieces, castling rights, en passant file, and side to move.
- **Transposition Table (TT)**: 64-bit cluster-based hash table supporting exact, lower bound (alpha), and upper bound (beta) flags.
- **Material Evaluation**: Hand-crafted base piece value tables.
- **Piece-Square Tables (PST)**: Color-mirrored positional square valuations for all piece types.
- **Draw Detection**: Threefold repetition state history verification and fifty-move rule checks.

---

## [0.4.0] - 2026-07-02

### Added
- **Negamax Search Core**: Recursive Alpha-Beta lookahead driver.
- **Iterative Deepening**: Progressive depth calculation with Principal Variation (PV) tracking and extraction.
- **Time Management System**: `TimeManager` and `SearchLimits` calculating optimal per-move allocations (soft and hard clock boundaries).
- **Search Controller**: Asynchronous `SearchController` with atomic `std::atomic<bool>` stop tokens for responsive GUI interruption.
- **UCI Protocol Output**: Telemetry reporting (`info depth`, `score cp`, `nodes`, `nps`, `time`, `pv`).

---

## [0.3.0] - 2026-06-20

### Added
- **Legal Move Generation**: Complete legal move validation pipeline filtering out pseudo-legal moves exposing the King.
- **Ray Attack Tables**: Orthogonal and diagonal precalculated bitboard sliding ray tables.
- **Knight and King Leaper Tables**: Precalculated jump masks for instantaneous attack queries.
- **Pawn Push & Capture Matrices**: Single/double push logic, diagonal pawn captures, en passant, and promotions.
- **Check Detection & Pin Analysis**: Authoritative `isSquareAttacked` and `inCheck` state inspectors.
- **Perft Validation Engine**: Recursive move path enumeration framework with divide diagnostics.

---

## [0.2.0] - 2026-06-10

### Added
- **Core Bitboard Architecture**: Strongly-typed 64-bit integer masks (`Bitboard`) representing board occupancy.
- **Core Domain Types**: Strongly-typed `Square`, `Color`, `Piece`, `CastlingRights`, and `Move` representations.
- **Position Abstraction**: `Position` class maintaining 12 piece bitboards and 3 composite occupancy masks with strict value semantics.
- **Transactional Move State Machine**: `MoveExecutor` with `UndoState` caching for deterministic `makeMove` / `undoMove` rollback.
- **FEN Parser**: Six-stage decoupled parser converting FEN strings into verified `Position` objects.
- **Board Debug Printer**: Dual-mode Unicode ASCII and terminal board visualizer.

---

## [0.1.0] - 2026-06-01

### Added
- **Milestone 0 Platform Genesis**: Initial project initialization and architectural charter.
- **Build Infrastructure**: Modern CMake 3.25+ configuration enforcing strict C++23 standards.
- **Compiler Diagnostic Controls**: Enforced zero-warning policy (`/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang).
- **Directory Scaffolding**: Modular directory structure (`engine/src/`, `engine/include/`, `tests/`, `scripts/`, `tools/`, `docs/`).
- **Architectural Charter & Vision**: Platform vision, core architectural boundaries, master roadmap, immutable engineering principles, and C++23 coding standards.
- **UCI Skeleton**: Entry-point `main.cpp` establishing basic UCI protocol handshakes (`uci`, `isready`, `quit`).
