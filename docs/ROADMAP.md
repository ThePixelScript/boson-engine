# Engine Development Roadmap

This document outlines the architectural roadmap for the Boson chess engine, tracking progress from platform genesis through advanced research milestones.

---

## Milestone Summary

| Milestone | Designation | Primary Focus | Status |
| :---: | :--- | :--- | :---: |
| **0** | **Platform Genesis & Architecture Charter** | Infrastructure, C++23 standards, directory layout, UCI skeleton | **Complete** |
| **1** | **State Processing & FEN Parsing** | Bitboards, domain primitives, FEN parser, board visualizer | **Complete** |
| **2** | **Move Generation & Validation** | Attack tables, legal move filtering, pin detection, Perft harness | **Complete** |
| **3** | **Search Core & Negamax Engine** | Alpha-Beta Negamax, iterative deepening, PV tracking, time allocation | **Complete** |
| **4** | **Transposition Table & Hashing** | 64-bit incremental Zobrist hash, TT cluster storage, draw detection | **Complete** |
| **5** | **Quiescence Search & Move Ordering** | Stand-pat quiescence, MVV-LVA, Killer moves, Global History | **Complete** |
| **6** | **Advanced Search Heuristics** | SEE, NMP, LMR, Counter-Move History, Continuation History, Correction History | **Complete** |
| **7** | **Positional Evaluation & Tuning** | Tapered evaluation, pawn structures, king safety, mobility, Texel tuning | Planned |
| **8** | **Neural Network Evaluation (NNUE)** | HalfKP/HalfKAv2 inference, SIMD vectorization (AVX2/AVX-512), dual evaluator | Planned |
| **9** | **Parallel Search (Lazy SMP)** | Lockless shared TT, thread pool, scaling telemetry | Planned |
| **10** | **Endgame Tablebases & Clock Policy** | Syzygy 3-4-5-6 probing (WDL/DTZ), dynamic complexity-based clock management | Planned |
| **Ω** | **Platform Horizon & Self-Play** | Distributed self-play pipeline, reinforcement learning, automated SPRT cluster | Research |

---

## Milestone 0: Platform Genesis & Architecture Charter
- **Scope:** Establish project scaffolding, strict compiler diagnostic standards, and architectural blueprints.
- **Key Deliverables:**
  - CMake 3.25+ build system enforcing C++23 (`set(CMAKE_CXX_STANDARD 23)`).
  - Strict compiler diagnostic flags (`/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang).
  - Clean directory layout: `engine/src/`, `engine/include/`, `tests/`, `scripts/`, `tools/`, `docs/`.
  - Architecture specifications, coding standard, five immutable principles, and platform vision.
  - Basic UCI handshake loop (`uci`, `isready`, `quit`).
- **Exit Criteria:** Zero compiler warnings across all targets; working test runner executable.
- **Status:** Complete.

---

## Milestone 1: State Processing & FEN Parsing
- **Scope:** Define core board primitives and state representation.
- **Key Deliverables:**
  - 64-bit `Bitboard` structures and basic bit-manipulation primitives (`std::popcount`, `std::countr_zero`).
  - Strongly typed domain entities: `Square`, `Color`, `Piece`, `CastlingRights`.
  - `Position` class tracking 12 piece bitboards and 3 composite occupancy bitboards.
  - Six-stage decoupled `FenParser` and dual-mode ASCII/Unicode `BoardPrinter`.
- **Exit Criteria:** 100% roundtrip accuracy parsing and formatting standard FEN strings.
- **Status:** Complete.

---

## Milestone 2: Move Generation & Validation
- **Scope:** Implement attack logic, pseudo-legal generation, and strictly legal move validation.
- **Key Deliverables:**
  - Precalculated sliding piece ray tables (orthogonal and diagonal).
  - Precalculated knight and king leaper masks.
  - Pawn push, capture, promotion, and en-passant mechanics.
  - Fast square attack queries (`isSquareAttacked`) and check detection (`inCheck`).
  - Transactional move state transitions (`makeMove` / `undoMove` with `UndoState`).
  - Recursive Perft validation harness with `divide` diagnostics.
- **Exit Criteria:** 100% node count match across standard Perft test suites up to depth 6.
- **Status:** Complete.

---

## Milestone 3: Search Core & Negamax Engine
- **Scope:** Implement fundamental lookahead tree search and search controller.
- **Key Deliverables:**
  - Recursive Alpha-Beta Negamax search framework.
  - Iterative deepening driver with Principal Variation (PV) tracking and extraction.
  - `TimeManager` computing soft and hard time budgets based on remaining clock and increment.
  - `SearchController` with atomic stop flags for non-blocking GUI interruption.
  - Standard UCI output formatting (`info depth`, `score cp`, `nodes`, `nps`, `time`, `pv`).
- **Exit Criteria:** Clean iterative deepening search under hard clock constraints with zero memory leaks.
- **Status:** Complete.

---

## Milestone 4: Transposition Table & Hashing
- **Scope:** Cache previously evaluated search subtrees to reduce redundant tree traversal.
- **Key Deliverables:**
  - 64-bit pseudorandom incremental Zobrist hash generator (pieces, castling, en passant, side).
  - Hash table with cluster storage and depth-preferred replacement policy.
  - Bound storage: exact score, lower bound (beta cutoff), upper bound (fail low).
  - Draw detection: threefold repetition and fifty-move rule checks.
  - Hand-crafted baseline material and piece-square table (PST) evaluation.
- **Exit Criteria:** Significant node count reduction on deep searches; zero hash collisions in validation runs.
- **Status:** Complete.

---

## Milestone 5: Quiescence Search & Move Ordering Foundation
- **Scope:** Mitigate the horizon effect and establish stage-based move ordering.
- **Key Deliverables:**
  - Quiescence search evaluating non-quiet positions (captures, promotions) to tactical stability.
  - Stand-pat delta pruning within quiescence search.
  - Move ordering pipeline: Hash move $\rightarrow$ MVV-LVA captures $\rightarrow$ Killer moves (2 slots/ply) $\rightarrow$ Global History heuristic.
- **Exit Criteria:** Resolution of tactical blunders at search horizon; measurable increase in beta-cutoff rate.
- **Status:** Complete.

---

## Milestone 6: Advanced Search Heuristics Suite
- **Scope:** Integrate modern selective search pruning, reductions, and history-based ordering.
- **Key Deliverables:**
  - Static Exchange Evaluation (SEE) recursive swap algorithm to classify winning/losing captures.
  - Null Move Pruning (NMP) with dynamic reduction ($R=2$) and material zugzwang guards.
  - Late Move Reductions (LMR) using logarithmic reduction tables and null-window scout re-searches.
  - Counter-Move History (CMH) table indexing refutations against previous moves.
  - 2-Ply Continuation History matrix tracking move efficacy within piece sequence contexts.
  - Correction History table dynamically adjusting static evaluation scores based on search results.
  - Aspiration Windows framework ($\pm 30$ cp) with progressive widening.
- **Exit Criteria:** Node count reduction of $>50\%$ on standard tactical suites while maintaining or improving solve rates.
- **Status:** Complete.

---

## Phase 6.5: Classical Search Enhancements Suite
- **Scope:** Modernize alpha-beta negamax with interior node scouting, pull-based staged move picking, pre-move pruning, and history-guided reduction modulation.
- **Key Modules:**
  - **Phase 6.5-A (Principal Variation Search & Zero-Window Scouting):** Full-window search on PV moves, zero-window scout searches on sibling candidates, and full-window re-searches on fail-highs. (Depth-6 Benchmark: 251,466 nodes).
  - **Phase 6.5-B (Staged MovePicker State Machine):** Pull-based staged legal move generator yielding candidate moves lazily across 8 stages (TTMove $\rightarrow$ GoodCaptures $\rightarrow$ EqualCaptures $\rightarrow$ Killers $\rightarrow$ CounterMoves $\rightarrow$ Quiets $\rightarrow$ BadCaptures $\rightarrow$ Delay). (Depth-6 Benchmark: 243,365 nodes).
  - **Phase 6.5-C (Reverse Futility Pruning):** Static evaluation pre-move cutoff at non-PV frontier nodes (depth 1–3, $M = 75 \times \text{depth}$) with strict TT write avoidance on cutoff. (Depth-6 Benchmark: 207,068 nodes).
  - **Phase 6.5-D (The Improving Heuristic - LMR-Only Modulation):** Search stack dynamic evaluation tracking ($staticEval > ss[ply - 2].staticEval$) modulating quiet late-move reductions ($r \leftarrow baseReduction + 1$ when improving, $r \leftarrow \max(0, baseReduction - 1)$ when non-improving; safety clamped to $r \le \text{depth} - 2$).
- **Benchmark Result:** Depth-6 isolated benchmark produces **313,092 nodes** (+51.2% vs Phase 6.5-C baseline of 207,068 nodes, reflecting deeper search on non-improving lines).
- **Strength Validation:** 100-game color-balanced match at 50ms/move: 50.0 / 100 (+13 =74 -13, $\Delta\text{Elo} = 0.0 \pm 67.7$ at 95% CI; SPRT LLR: -0.04), noting no statistically significant strength effect detected at 50ms movetime.
- **Status:** **Complete**. Formally tagged and frozen at `v0.9.5-classical-enhanced`.

---

## Milestone 7: Positional Evaluation & Automated Tuning
- **Scope:** Expand positional knowledge, abstract evaluation architecture, and automate parameter optimization.
- **Phases:**
  - **Phase 7-A (Evaluation Abstraction):** Decouple search engine core from concrete evaluation logic via an abstract polymorphic interface (`boson::eval::IEvaluator`) and concrete adapter (`boson::eval::ClassicalEvaluator`).
    * **Architectural Invariant:** Search is fully decoupled behind `IEvaluator` dynamic dispatch; zero `ParameterRegistry` or UCI mutations; exact bit-for-bit depth-6 benchmark node parity (313,092 nodes == 313,092 nodes, $\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Status:** **Complete**.
  - **Phase 7-B (HalfKP Feature Infrastructure & Topology Specification):** Implemented canonical 40,960-feature HalfKP sparse feature transformer with scratch feature generation and differential move delta computation supporting quiets, captures, 16 promotion variants, 4 castling paths, en-passant, and opponent king captures.
    * **Contract & Verification:** Verified complete feature-index domain coverage with no collisions under the canonical indexing scheme ($\text{Index} \in [0, 40959]$) across all 64 king squares, 10 piece codes, and 64 piece squares. Suite #29 passed 11/11 validation gates, including a 10,000-ply / 20,000-perspective delta equivalence oracle ($(\text{Features}(P_{\text{before}}) \setminus \text{removed}) \cup \text{added} == \text{Features}(P_{\text{after}})$).
    * **Architectural Invariant:** Standalone evaluative infrastructure; zero search or parameter modifications; exact depth-6 benchmark node count parity maintained (313,092 nodes == 313,092 nodes, $\Delta = 0$ vs frozen `v0.9.5-classical-enhanced` control).
    * **Status:** **Complete**.
  - **Phase 7-C (Dual-Perspective Incremental Accumulator):** Implemented 64-byte aligned dual-perspective accumulator structures (`AccumulatorHalf`, `Accumulator`) and an incremental accumulator stack (`AccumulatorStack`, capacity 128) directly consuming Phase 7-B feature transition deltas.
    * **Contract & Verification:** Verified full rebuild oracle ($A_{\text{incremental}} \equiv A_{\text{rebuild}}$ bit-for-bit) across quiet moves, normal captures, all 16 promotion variants, 4 castling paths, and en-passant. Verified Per-Perspective King Rule ($\text{RebuildPerspective}(c) \iff K_c^{\text{before}} \ne K_c^{\text{after}}$) ensuring moving king perspective rebuilds from scratch while non-moving king perspective updates incrementally via feature deltas.
    * **Arithmetic Safety & Stack Lifecycle:** Intermediate accumulator calculations strictly executed in `int32_t` with explicit, range-safe narrowing to `int16_t`; bounded test weights eliminate signed overflow UB. Multi-ply reversible random walks confirmed push/pop correctness with bit-exact root accumulator and position state restoration. Suite #30 passed 10/10 acceptance gates.
    * **Architectural Invariant:** Standalone evaluation infrastructure; zero modifications to `Position`, `Search`, `SearchStack`, `MovePicker`, `ClassicalEvaluator`, or `ParameterRegistry`. Depth-6 isolated benchmark locked at exactly 313,092 nodes ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control). Scalar accumulator/rebuild path established as frozen reference oracle.
    * **Status:** **Complete**.
  - **Phase 7-D (Scalar NNUE Evaluation Primitives & Network Representation):** Implemented standalone feed-forward neural network model representation, quantized integer activation clipping (CReLU [0, 127]), binary serialization, and bit-exact scalar inference reference pipeline consuming dual-perspective accumulators.
    * **Mathematical Pipeline:** Accumulator (1024 combined) $\to$ CReLU [0, 127] $\to$ FC1 ($1024 \to 32$) $\to$ /64 (trunc toward zero) $\to$ CReLU [0, 127] $\to$ FC2 ($32 \to 32$) $\to$ /64 (trunc toward zero) $\to$ CReLU [0, 127] $\to$ FC3 ($32 \to 1$) $\to$ $\times 16$ output scale $\to$ explicit clamping $[-30000, +30000]$.
    * **Golden Vector Parity:** Hardcoded intermediate layer diagnostics verified bit-for-bit on startpos (seed 1337 model): FC1 raw `{18882, -7938, -36913, 24244}`, FC1 activated `{127, 0, 0, 127}`, FC2 raw `{13394, -1099, -3180, -1241}`, FC2 activated `{127, 0, 0, 0}`, FC3 raw `293`, Final Score `4688` cp.
    * **Memory Safety & Ownership:** `NetworkModel` manages large `FeatureWeights` (~41.9 MB) via `std::unique_ptr` with explicitly deleted copy constructor and copy assignment operators; zero copy overhead during forward inference.
    * **Fail-Fast Serialization:** Little-endian binary byte stream format with validation of magic (`0x4E4E5545`), version (`1`), and network dimensions rejecting malformed headers cleanly prior to payload allocation.
    * **Arithmetic Safety:** Truncation-defined integer division activation scaling (`/ 64`) and extreme boundary validation across `{-32768, -1, 0, 1, 127, 128, 32767}` confirming overflow safety and strict activation clamping. Suite #31 passed 14/14 acceptance gates.
    * **Architectural Invariant:** Zero modifications to `Position`, `Search`, `SearchStack`, `MovePicker`, `ClassicalEvaluator`, `TranspositionTable`, or `ParameterRegistry`. Classical depth-6 benchmark locked at exactly 313,092 nodes ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Status:** **Complete**.
  - **Phase 7-E (Automated Tuning - Texel Tuner):** Offline Texel Tuning implementation in `tools/` optimizing evaluation weights against grandmaster game datasets.
- **Exit Criteria:** Statistically significant Elo gain against baseline in fixed-depth SPRT matches.
- **Status:** In Progress (Phases 7-A, 7-B, 7-C, & 7-D Complete).

---

## Milestone 8: Neural Network Evaluation (NNUE)
- **Scope:** Integrate an Efficiently Updatable Neural Network inference engine alongside HCE.
- **Key Deliverables:**
  - NNUE inference engine supporting standard network architectures (HalfKP / HalfKAv2).
  - Incremental accumulator maintenance integrated into `MoveExecutor::makeMove` and `undoMove`.
  - SIMD-vectorized forward pass using AVX2 and AVX-512 integer arithmetic.
  - Dual-evaluator configuration via UCI option (`Use NNUE = true/false`).
- **Exit Criteria:** Fast forward-pass inference (>2M NPS with NNUE); measurable Elo leap over HCE.
- **Status:** Planned.

---

## Milestone 9: Parallel Search (Lazy SMP)
- **Scope:** Scale search across multi-core CPU architectures.
- **Key Deliverables:**
  - Multi-threaded search orchestrator using Lazy SMP architecture.
  - Lockless Transposition Table updates using atomic 64-bit verification words.
  - Thread-safe `SearchController` with unified stop coordination.
  - Thread scaling benchmarks (1, 2, 4, 8, 16 threads).
- **Exit Criteria:** Linear or near-linear depth scaling across core counts with zero deadlocks or race conditions.
- **Status:** Planned.

---

## Milestone 10: Endgame Tablebases & Dynamic Clock Policy
- **Scope:** Tablebase probing and dynamic time management.
- **Key Deliverables:**
  - Syzygy 3-4-5-6 piece endgame tablebase probing (WDL and DTZ).
  - Root probing and in-search probing with distance-to-zero pruning.
  - Position complexity metrics dynamically modulating time allocation (e.g., extend time when search score is unstable or root move changes frequently).
- **Exit Criteria:** Immediate 100% accurate play in probed endgame positions; improved time distribution in complex middlegames.
- **Status:** Planned.

---

## Milestone Ω: Platform Horizon & Research Framework
- **Scope:** Autonomous self-play, reinforcement learning, and distributed infrastructure.
- **Key Deliverables:**
  - High-throughput self-play match runner executing automated SPRT testing clusters.
  - Reinforcement learning pipeline training evaluation weights from scratch.
  - Hardware micro-profiling harnesses for hardware-specific optimizations.
- **Exit Criteria:** Continuous automated self-improvement and statistical testing pipeline.
- **Status:** Research.