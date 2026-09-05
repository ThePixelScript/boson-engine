# Engine Vision and Research Platform Scope

## 1. Platform Purpose

Boson is an experimental chess engine platform built in C++23. It serves as a testbed for investigating high-performance systems programming, hardware-level bitboard algorithms, lookahead search heuristics, and evaluation architectures.

While chess is the domain, the broader research objective is to explore how modern C++ abstractions can coexist with zero-overhead hardware utilization (BMI2, AVX2, SIMD vectorization, and cache line optimization). Boson is structured to support multi-decade experimentation without suffering from the architectural rot typical of long-lived engine codebases.

---

## 2. Why Correctness Precedes Speed

In chess programming, speed is a multiplier on correctness. If correctness is zero, the result is zero.

- **Non-Linear Error Compounding:** A subtle bug in move generation (such as missing an en-passant pin or mishandling king attack masks) will not merely cause a missed move; it corrupts alpha-beta bounds, poisons the transposition table with invalid cutoffs, and destabilizes subsequent search depths.
- **Reproducibility Over Cleverness:** Fast code that cannot be deterministically reproduced or tested in isolation makes performance tuning impossible. In Boson, every state transition, perft enumeration, and heuristic cutoff must be strictly deterministic and testable.
- **Refactoring Resilience:** An engine built on provably correct abstractions can be aggressively refactored, parallelized, or migrated to new instruction sets with confidence that behavioral deviations will be caught immediately by the verification suite.

---

## 3. Core Architectural Vectors

Boson is built around three primary design vectors:

### Modular Subsystem Boundaries
Monolithic engines blend evaluation terms into search loops and scatter board mutations across dozens of heuristics. Boson enforces strict interface boundaries:
- The board layer maintains state invariants and executes moves transactionally.
- The evaluation layer scores a board view without knowing how the search tree was traversed.
- The search engine manages lookahead and tree pruning without knowing how piece values are derived.
- The UCI layer translates external commands without direct access to bitboards or search state.

### Hardware-Native Data Layouts
Modern CPUs spend more cycles stalled on memory than performing arithmetic. Boson organizes data structures for cache efficiency:
- Board state fits within compact, contiguous bitboard arrays.
- History and counter-move tables are indexed directly by compact types (`Piece`, `Square`) for fast L1/L2 cache line hits.
- Transposition table entries are clustered and aligned to 64-byte cache lines.

### Replaceable Evaluation Paradigms
The evaluation pipeline is decoupled from the tree search. Boson begins with a Hand-Crafted Evaluation (HCE) engine using tapered piece-square tables and material scores, coupled with an empirical Correction History table. The architecture is explicitly designed to allow a drop-in transition to an Efficiently Updatable Neural Network (NNUE) without requiring modifications to the alpha-beta core.

---

## 4. Long-Term Experimentation Goals

1. **Evaluation Architecture:** Evolve from classical HCE with Texel tuning to an optimized NNUE inference engine with HalfKP/HalfKAv2 architectures, utilizing AVX2/AVX-512 SIMD vectorization.
2. **Search Heuristics:** Explore the interactions between recursive swap evaluations (SEE), history heuristics (Continuation History, Counter-Move History), and dynamic pruning thresholds (LMR, NMP).
3. **Parallel Lookahead:** Implement scalable, lockless multi-threaded search (Lazy SMP) using atomic memory operations for transposition table access.
4. **Endgame Integration:** Integrate Syzygy tablebase probing (WDL and DTZ) directly into root and search nodes.
5. **Autonomous Verification:** Establish an automated testing pipeline capable of executing high-volume SPRT matches to validate every algorithmic addition with statistical confidence.