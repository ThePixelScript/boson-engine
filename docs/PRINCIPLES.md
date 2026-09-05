# Engineering Principles

This document defines the five core principles governing the development of the Boson chess engine. Every design choice, code modification, and refactoring effort must align with these rules.

---

## 1. Correctness Before Speed

A fast search routine or move generator is useless if it introduces subtle bugs or state corruption.

- **State Invariants:** The `Position` abstraction must never be left in an inconsistent state. Move execution via `MoveExecutor::makeMove` and rollback via `MoveExecutor::undoMove` must preserve exact bitboard counts, occupancy masks, castling rights, and incremental hash keys.
- **Verification First:** Move generation correctness is validated via recursive Perft suites before optimization. No attack generator or board mechanic may be optimized until baseline node counts match established reference values across diverse test positions.
- **Assertions in Debug:** Use runtime assertions in debug builds (`_DEBUG`) to validate pre-conditions, post-conditions, and bounds. Debug builds must fail immediately when an invariant is violated rather than masking undefined behavior.

---

## 2. Architecture Before Optimization

Premature micro-optimization damages code structure and creates maintenance bottlenecks. Clean module boundaries must be established before hot-path profiling.

- **Unidirectional Layering:** Module dependencies flow in one direction:
  `UCI (Application) -> Search -> Evaluation -> Board Core`
  Lower layers never reference higher layers. The board representation has no knowledge of search heuristics or UCI commands.
- **Encapsulated Mechanics:** Specialized algorithms (such as Static Exchange Evaluation, Late Move Reductions, or Transposition Tables) belong in dedicated modules with explicit APIs, rather than being inlined into monolithic search loops.
- **Value Semantics and Lifetime Control:** State objects use value semantics and stack allocation where possible. Avoid unnecessary heap allocations, raw pointers, and complex reference topologies that complicate thread-safety and cache locality.

---

## 3. Measure Before Changing

Performance claims and algorithmic adjustments must be backed by empirical measurement. Intuition regarding superscalar CPU performance and tree search pruning is frequently incorrect.

- **Deterministic Benchmarking:** Changes to search algorithms, pruning parameters, or move ordering must record fixed-depth node counts, time-to-depth, and NPS across standardized test suites.
- **Empirical Validation:** Search pruning heuristics (such as LMR or Null Move Pruning) reduce node counts but can introduce tactical blindness. All heuristic modifications must be verified through automated tactical suites (e.g., Win-At-Chess) and match testing under realistic time controls.
- **Benchmark Ledger:** Significant performance milestones and baseline measurements must be recorded in `docs/BENCHMARKS.md` along with compiler versions, target architecture, and configuration flags.

---

## 4. Replaceable Modules

Boson is designed as a long-term research platform. Subsystems must be decoupled so they can be replaced or upgraded independently without requiring cascading refactors.

- **Evaluation Interface:** The evaluation subsystem exposes a clean interface returning a centipawn score from the perspective of the side to move. The search engine is agnostic to whether evaluation is computed via hand-crafted tables (HCE), an NNUE neural network, or a hybrid model.
- **Move Generation Abstraction:** Sliding ray lookups, attack bitboards, and legality filtering reside inside `MoveGenerator` and `Position`. Upgrading from precalculated ray attacks to hardware PEXT bitboards requires no changes to search or evaluation.
- **Protocol Decoupling:** The engine core is decoupled from the communication protocol. The UCI parser acts strictly as a translation layer between standard I/O streams and the engine's internal controllers.

---

## 5. Every Commit Improves Boson

Quality is maintained continuously throughout development, not deferred to stabilization phases.

- **Zero-Warning Compilation:** All code must compile with zero warnings under strict diagnostic flags:
  - MSVC: `/W4 /WX /permissive-`
  - GCC / Clang: `-Wall -Wextra -Wpedantic -Werror`
  No compiler warnings are permitted to land in the repository.
- **No Regressions:** Every change must compile cleanly and pass all active unit tests and diagnostic checks.
- **Focused Changes:** Commits should address a single logical change, bug fix, or feature, accompanied by clear technical descriptions and documentation updates.
