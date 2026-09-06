# Changelog

All notable changes to the Boson Chess Engine project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- **Milestone 8: Neural Network Training Pipeline & Empirical Model Progression**
  - High-throughput self-play dataset harvesting engine for quiescence-resolved FEN generation.
  - PyTorch/LibTorch supervised HalfKP trainer with clipped ReLU activation and custom sigmoid loss.
  - Little-endian binary quantization tool ($W \times 64$, intermediate clamp $[0, 127]$, output scale $\times 16$).
  - Sequential validation of candidate networks (`boson-v2.nnue`, `boson-v3.nnue`) under automated SPRT matches.
- **Milestone 9: Multithreading & SMP Parallel Search** (Lazy SMP, Lockless Shared TT).
- **Milestone 10: Endgame Tablebases & Dynamic Clock Policy** (Syzygy 3-4-5-6 piece probing).
- **Milestone Ω: Autonomous Self-Play Framework & Research Platform**.

---

## [0.9.1] - 2026-09-07

### Phase 7-GB: First Empirical NNUE Strength Experiment
- **Empirical Match Execution:** Completed 170 games across 50 opening blocks (85 pairings) under 4-game alternating color cadence ($10\text{ s} + 0.1\text{ s increment}$).
- **Protocol & Process Stability:** 100% stable execution with 0 engine crashes, 0 unhandled timeouts, 0 memory faults, and 0 illegal moves across 7,095 plies. Perfect color symmetry (85 White wins, 85 Black wins).
- **Match Outcome:** Candidate-NNUE scored $0.0 / 170.0$ ($0.0\%$, +0 =0 -170); all games terminated naturally via checkmate (average length 41.74 plies). Wilson 95% score confidence interval: $[0.0\%, 2.2\%]$.
- **Statistical Metric Analysis:** Descriptive logistic $\Delta\text{Elo} = -2400.0\text{ Elo}$ (clamped at numerical domain floor $\varepsilon_{\text{logistic}} = 10^{-6}$); delta-method 95% CI: $[-28514.2, +23714.2]\text{ Elo}$.
- **Wald SPRT Decision:** Terminated early with **FAIL ($H_0$ Accepted)** at $\text{LLR} = -4.96$ ($H_0: 0.0, H_1: +10.0\text{ Elo}, \alpha=0.05, \beta=0.05$; crossing lower rejection bound $A = -2.9444$ at Game 101).
- **Opening Sensitivity:** 0.00% score variance across all tested opening blocks, confirming uniform evaluation deficiency across open, semi-open, and closed structures.
- **Root Cause Determination:** Confirmed complete functional integrity of AVX2 inference pipeline and accumulator stack; empirical failure is strictly localized to the untrained synthetic weight distribution of `boson-v1.nnue`.
- **Classical Search Invariance:** Depth-6 benchmark locked at exactly **313,092 nodes** ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).

---

## [0.9.0] - 2026-09-06

### Milestone 7: Neural Network Evaluation (NNUE) Infrastructure & Inference Engine
- **Phase 7-A (Evaluation Abstraction):** Introduced polymorphic `IEvaluator` interface decoupling search from concrete evaluation; implemented `ClassicalEvaluator` adapter. Enforced universal side-to-move centipawn scoring contract.
- **Phase 7-B (HalfKP Feature Transformer):** Implemented canonical 40,960-feature HalfKP sparse feature representation with rank-flipped Black perspective mirroring ($sq \oplus 56$), scratch feature generation, and differential move delta tracking for all move types.
- **Phase 7-C (Dual-Perspective Incremental Accumulator):** Added 64-byte aligned `AccumulatorHalf` and `Accumulator` structures with capacity-128 `AccumulatorStack`. Verified Per-Perspective King Rule and range-safe 32-bit arithmetic eliminating signed overflow UB.
- **Phase 7-D (Scalar NNUE Inference & Network Model):** Implemented feed-forward neural network representation, quantized integer activation clipping (CReLU $[0, 127]$), division-by-64 truncation scaling, little-endian binary serialization, and bit-exact scalar reference inference oracle.
- **Phase 7-E (Search Integration Lifecycle):** Integrated explicit evaluator lifecycle hooks (`initializeSearch`, `notifyMove`, `notifyUndo`) into negamax, null-move pruning, and quiescence loops. Proved null-move perspective invariance (Gate 7-E-11). Exposed `Eval_Mode` via `ParameterRegistry` with automatic TT flush on mode transition.
- **Phase 7-F (AVX2 SIMD Vectorized Inference):** Optimized accumulator updates (`AVX2Accumulator`) and forward pass (`AVX2Inference`) with runtime CPUID dispatch. Achieved 9.07x speedup (1.12M NPS) with bit-for-bit intermediate layer parity verified over 10,000 diverse reachable positions.
- **Phase 7-GA (Strength Validation Harness & SPRT):** Implemented fail-fast CLI (`--require-nnue`), FIPS 180-4 SHA-256 validator, 50-opening canonical book with 4-game alternating cadence, decoupled Wilson/logistic statistics, and Wald SPRT sequential framework. All 13 validation gates passed (Suite #34).

---

## [0.8.0] - 2026-09-05

### Milestone Omega & Phase 6.5: Classical Search Enhancements Suite
- **Phase 6.5-A (Principal Variation Search & Scouting):** Implemented full-window search on PV moves, zero-window scout searches ($[-\alpha-1, -\alpha]$) on non-PV moves, and full-window re-searches on fail-highs.
- **Phase 6.5-B (Staged MovePicker State Machine):** Implemented pull-based lazy legal move generator yielding candidate moves across 8 prioritized stages (TTMove $\to$ GoodCaptures $\to$ EqualCaptures $\to$ Killers $\to$ CounterMoves $\to$ Quiets $\to$ BadCaptures $\to$ Delay).
- **Phase 6.5-C (Reverse Futility Pruning):** Implemented static evaluation pre-move cutoffs at non-PV frontier nodes (depth 1–3, $M = 75 \times \text{depth}$) with strict TT write avoidance on cutoff.
- **Phase 6.5-D (The Improving Heuristic):** Integrated search stack dynamic evaluation tracking ($staticEval > ss[ply-2].staticEval$) modulating quiet late-move reductions ($r \leftarrow base + 1$ when improving, $r \leftarrow \max(0, base - 1)$ when non-improving).
- **Milestone Omega Quality Gates:** Added Deterministic Benchmark Harness (`BenchmarkRunner`, `BenchmarkCorpus`, `BenchmarkReporter`) with isolated/persistent modes, `ParameterRegistry` dynamic configuration, and comprehensive tactical and state integrity suites.
- **Frozen Classical Baseline:** Formally frozen and tagged at `v0.9.5-classical-enhanced`. Canonical depth-6 isolated benchmark locked at **313,092 nodes**.

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
