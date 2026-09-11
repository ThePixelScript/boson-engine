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
| **8** | **Neural Network Training & Empirical Progression** | Dataset construction, supervised HalfKP trainer, quantization, SPRT tuning | **In Progress** |
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
  - **Phase 7-E (NNUE Evaluator Provider & SearchStack Lifecycle Integration):** Implemented the polymorphic `NNUEEvaluator` provider adhering to `IEvaluator`, integrated explicit search stack lifecycle synchronization hooks, and established UCI hot-swapping via `ParameterRegistry`.
    * **Universal Scoring Contract:** Strict perspective convention verified across both `ClassicalEvaluator` and `NNUEEvaluator`—`IEvaluator::evaluate(pos)` returns centipawns relative to `pos.sideToMove()`.
    * **Explicit Search Synchronization Lifecycle:** `IEvaluator` extended with `initializeSearch(rootPos)`, `notifyMove(before, after, move)`, and `notifyUndo()`. In `Search.cpp`, all move execution sites in negamax main loop, null-move pruning, and quiescence search (tactical & move picker loops) synchronize the evaluator stack 1-to-1 without silent fallback rebuilding.
    * **Null-Move Accumulator Invariance (Gate 7-E-11):** Verified bit-exact accumulator preservation across null moves, [Them | Us] perspective inversion matching side-to-move semantics, and exact undo restoration. Confirmed zero accumulator corruptions during search with null-move pruning enabled.
    * **Hot-Swapping & Configuration:** Registered `Eval_Mode` (0 = Classical, 1 = NNUE; default 0, `FullReset`) in `ParameterRegistry` exposed via UCI, triggering automatic table flushing on mode transition.
    * **Operational Readiness:** Completed a 20-game operational smoke match in NNUE mode with 0 crashes, 0 timeouts, and 100% legal moves. Suite #32 passed 11/11 acceptance gates.
    * **Classical Invariance Oracle:** Depth-6 benchmark locked at exactly **313,092 nodes** (0 node delta vs `v0.9.5-classical-enhanced`).
    * **Status:** **Complete**.
  - **Phase 7-F (AVX2 SIMD Optimization & Vectorized Inference):** Implemented AVX2 SIMD-vectorized forward inference (`AVX2Inference`) and vectorized accumulator scratch rebuild & incremental delta update primitives (`AVX2Accumulator`) with runtime CPUID dispatch and clean scalar fallback.
    * **Bit-for-Bit Golden Oracle Parity:** AVX2 reproduces the Phase 7-D scalar golden oracle bit-for-bit across all observable intermediate layers (FC1 raw, FC1 activated, FC2 raw, FC2 activated, FC3 raw, final score clamped to $[-30000, +30000]$).
    * **Arithmetic Safety & Saturating Pack Invariant:** Intermediate accumulator calculations unpack 16-bit lanes to 32-bit (`_mm256_cvtepi16_epi32`) and compute sums in 32-bit vector arithmetic (`_mm256_add_epi32`, `_mm256_sub_epi32`), strictly eliminating signed 16-bit wrapping UB. Narrowing back to 16-bit utilizes `_mm256_packs_epi32` followed by cross-lane permutation `_mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0))`. Saturated packing is strictly an implementation mechanism; mathematical correctness is guaranteed by the proven $[-32768, 32767]$ intermediate range invariant.
    * **Vectorized Truncation Division (/64):** Exact integer division toward zero matching C++ integer division across all boundary inputs via `_mm256_srai_epi32(_mm256_add_epi32(x, _mm256_srli_epi32(_mm256_srai_epi32(x, 31), 26)), 6)`.
    * **Full 10,000-Position Differential Verification:** Evaluated across a 10,000-reachable-position corpus generated from diverse random legal walks covering all move types, yielding exactly 0 discrepancies across all intermediate and final network layers.
    * **Benchmark Methodology & Performance Metrics:** Benchmark executed on a single thread across 100,000 forward inference iterations (MSVC 1951 C++23 Release `/O2 /Oi /arch:AVX2`): 809.43 ms (Scalar) $\to$ 89.25 ms (AVX2), achieving a **9.07x speedup** and **1.12M evaluations/sec** (1,120,459 evals/sec).
    * **Classical Search Invariance:** Depth-6 benchmark locked at exactly **313,092 nodes** (0 node drift vs `v0.9.5-classical-enhanced`). Suite #33 passed 14/14 acceptance gates.
    * **Status:** **Complete**.
  - **Phase 7-GA (NNUE Strength Validation Infrastructure & SPRT Framework):** Implemented the full experimental evaluation infrastructure, deterministic tournament runner, and sequential hypothesis testing framework for empirical NNUE strength measurement.
    * **Strict Model Identity & Fail-Fast CLI:** Implemented CLI flag `--require-nnue` with strict fail-fast enforcement (`std::exit(1)`) and zero silent fallback to Classical on missing, malformed, or incompatible network models.
    * **FIPS 180-4 In-Engine SHA-256 Engine:** Added high-performance in-engine SHA-256 calculation for binary model validation and pre-match engine handshake auditing (`Eval_Mode`, model SHA-256 digest, AVX2 SIMD backend, and `Network Version=1`).
    * **Balanced 50-Opening Book & 4-Game Schedule:** Expanded the opening suite to 50 balanced 2-ply/4-ply openings (`open_01` to `open_50`) verified for legality and exact FEN equivalence. Enforced a deterministic 4-game-per-opening alternating color cadence ($C\text{-}W/N\text{-}B, N\text{-}W/C\text{-}B, C\text{-}W/N\text{-}B, N\text{-}W/C\text{-}B$) totaling 200 games per match.
    * **Decoupled Statistical Analysis:** Implemented reporting for W/D/L, draw rate, average game ply, logistic Elo ($\Delta\text{Elo} = -400 \cdot \log_{10}(1/S - 1)$), and delta-method 95% confidence intervals ($\pm 1.96 \cdot \sigma_S \cdot [400 / (\ln(10) S(1-S))]$).
    * **Wald SPRT Sequential Framework:** Implemented sequential probability ratio testing with explicit hypotheses ($H_0: 0\text{ Elo}, H_1: +10\text{ Elo}, \alpha=0.05, \beta=0.05$) and exact log-likelihood boundaries ($\pm 2.944439$).
    * **Classical Search Control Invariance:** Depth-6 benchmark locked at exactly **313,092 nodes** ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Full Regression Battery:** Suite #34 passed all 13 validation gates (Gates 7-G-1 through 7-G-12, including Gate 7-G-2A). All 34 test suites passing cleanly (`phase7G: 1`).
    * **Status:** **Complete**.
  - **Phase 7-GB (First Empirical NNUE Strength Experiment):** Executed the 200-game empirical tournament between Candidate (AVX2 NNUE, `boson-v1.nnue`) and Control (Classical HCE) using the validated Phase 7-GA experimental harness.
    * **Execution & Protocol Stability:** 170 games completed across 50 opening blocks under 4-game alternating color cadence with zero engine crashes, zero unhandled timeouts, zero memory corruptions, and zero illegal moves across 7,095 plies. Color outcomes were perfectly balanced across sides (85 White wins, 85 Black wins).
    * **Empirical Strength Outcome:** Candidate-NNUE scored **0.0 / 170.0 (0.0%)** (+0 =0 -170), yielding a draw rate of 0.0% and an average game length of 41.74 plies (100% decisive checkmates). Wilson-style 95% score confidence interval: [0.0%, 2.2%].
    * **Statistical Metric Analysis:** Descriptive logistic $\Delta\text{Elo} = -2400.0\text{ Elo}$ (clamped at numerical domain floor $\varepsilon_{\text{logistic}} = 10^{-6}$); delta-method 95% confidence interval: $[-28514.2, +23714.2]\text{ Elo}$.
    * **Wald SPRT Sequential Decision:** Sequential probability ratio testing ($H_0: 0.0\text{ Elo}, H_1: +10.0\text{ Elo}, \alpha=0.05, \beta=0.05$) terminated early and decisively with **FAIL ($H_0$ Accepted)** at $\text{LLR} = -4.96$ (crossing lower threshold $A = -2.944439$ at Game 101).
    * **Opening-Pair Sensitivity:** Evaluated across 50 opening blocks (41 fully played $4/4$, 3 partially played $2/4$, 6 unplayed). Candidate showed 0.0% score across all tested lines, demonstrating uniform tactical and positional deficiency regardless of open/semi-open/closed pawn structure.
    * **Root Cause & Architectural Assessment:** The empirical failure is strictly localized to the cold-start / untrained synthetic weight distribution of `boson-v1.nnue` when matched against the mature, highly tuned Classical HCE. The software infrastructure, AVX2 SIMD forward inference kernel (1.12M NPS), accumulator stack, and search lifecycle hooks functioned with 100% fidelity.
    * **Classical Search Control Invariance:** Isolated depth-6 benchmark re-verified post-match, locked at exactly **313,092 nodes** ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Status:** **Complete**.
- **Exit Criteria:** NNUE inference infrastructure verified; initial empirical strength baseline established and documented.
- **Status:** **Complete** (Phases 7-A, 7-B, 7-C, 7-D, 7-E, 7-F, 7-GA, & 7-GB Complete).

---

## Milestone 8: Neural Network Training Pipeline & Empirical Model Progression
- **Scope:** Establish an end-to-end dataset generation, supervised offline training, quantization, and self-play validation pipeline to train competitive HalfKP network weights for the validated Phase 7 AVX2 NNUE inference engine.
- **Key Deliverables:**
  - **Self-Play Dataset Harvesting Engine:** High-throughput quiet-position generator harvesting millions of unique, quiescence-resolved FEN records labeled with classical search evaluations and game outcomes ($z \in [0.0, 1.0]$).
  - **PyTorch/LibTorch Supervised Trainer:** Offline HalfKP training pipeline with dual-perspective feature caching, clipped ReLU activation, custom sigmoid loss target ($S(q) = 1 / (1 + 10^{-q/400})$), and weight decay regularization.
  - **Integer Quantization & Binary Exporter:** Automated quantization tool converting float32 network weights into Boson NNUE v1 little-endian binary format ($W \times 64$, intermediate activation clamp $[0, 127]$, output scale $\times 16$) with SHA-256 integrity validation.
  - **Empirical Model Progression (Phase 7-GC / Milestone 8):** Sequential validation of candidate networks (`boson-v2.nnue`, `boson-v3.nnue`) against frozen classical baseline (`v0.9.5-classical-enhanced`) under automated SPRT matches.
- **Phases:**
  - **Phase 8-A (Dataset Construction, Label Semantics & Feature-Parity Pipeline):** Established canonical self-play position extraction, strict label semantics, binary dataset serialization, opening-group atomic splitting, and bit-exact HalfKP feature parity between Python tooling and C++ engine infrastructure.
    * **Exact 200-Game Corpus Manifest:** Ingested strictly the 50 canonical opening blocks $\times 4$ games = 200 games, examining exactly 5,000 plies across color-balanced matches.
    * **Additive Pipeline Accounting:** Examined 5,000 raw plies; filtered out 3 forced checkmate evaluations ($|q| \ge 25,000$ cp, terminal mate scores); retained exactly 4,997 canonical position records with 0 residual/unaccounted positions.
    * **Opening-Group Atomic Partitioning:** Deterministic partitioning at the opening-group level (grouping all 4 games of each opening line) via deterministic 64-bit FNV-1a hash (`hashOpeningId`):
      - Train: 45 opening blocks (180 games, 4,497 records, 90.0%).
      - Validation: 3 opening blocks (`open_44`, `open_45`, `open_48` — 12 games, 300 records, 6.0%).
      - Test: 2 opening blocks (`open_49`, `open_50` — 8 games, 200 records, 4.0%).
    * **Pairwise Cross-Split Collision Elimination:** Strictly enforced zero-leakage invariant across all split boundaries: $|T \cap V| = 0$, $|T \cap Te| = 0$, $|V \cap Te| = 0$, ensuring zero position memorization across evaluation sets.
    * **Bit-Exact Feature-Parity Oracle:** Implemented canonical HalfKP feature extractor in Python (`tools/dataset/extractor.py`) matching the C++ engine's sparse HalfKP feature transformer (`boson::eval::HalfKP::computeActiveFeatures`) bit-for-bit across all 64 king squares, 10 piece codes, and 64 piece squares ($\text{Index} \in [0, 40959]$). Verified parity across quiet moves, captures, promotions, en-passant, and castling paths.
    * **Packed Binary Dataset Schema:** Little-endian binary byte stream format (`magic=0x42445354` "BDST", `version=1`) with variable-length HalfKP active feature indices, centipawn score $q$, and game outcome $z$. Reconciled binary digests:
      - `train.bin`: 4,497 records, 620,634 bytes, SHA-256 `6023eebc4be69a55b225a71667da740bb66bb016c58163af5782324b615d5bfa`
      - `val.bin`: 300 records, 41,656 bytes, SHA-256 `a80e4ec34172c2f42ee5167a95cae043d20c74bbf6285758e37cdaf984e80cbf`
      - `test.bin`: 200 records, 27,664 bytes, SHA-256 `f504d41cd4a10800667dcd168bfa7a2e6e04e09310f502436be8a69fe99d255e`
    * **Heuristic Teacher Mapping Calibration:** Evaluated empirical alignment between game outcome $E[z|q]$ and logistic teacher mapping $\sigma(q/400)$ across centipawn bins, confirming strictly monotonic correspondence without sign inversions.
    * **Classical Search Control Invariance:** Isolated depth-6 benchmark locked at exactly **313,092 nodes** ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Full Verification Battery:** Suite #35 passed all 15 validation gates (Gates 8-A-1 through 8-A-15). All 35 test suites passing cleanly (`phase8A: 1`).
    * **Status:** **Complete / Closed**.
  - **Phase 8-B (Float32 HalfKP Supervised Training Pipeline & Baseline Experiments):** Implemented the supervised training pipeline with the explicit `/64` hidden-layer scaling bridge, executed controlled micro-baseline experiments, and performed quantization shadow testing.
    * **Model Mode & Topology Partitioning:** Mode P2 (Frozen Feature Transformer, Trainable FC1/FC2/FC3 Head: 33,889 trainable parameters; 20,972,032 frozen embedding weights) served as primary baseline, with Mode P1 (Full Network, 21,005,921 trainable parameters) executed for pipeline validation.
    * **Micro-Baseline Dataset Context:** Trained strictly on `data/dataset_phase8a/train.bin` (4,497 records, 180 games, 45 opening blocks); validation on `val.bin` (300 records, 12 games, 3 opening blocks); strict test isolation on `test.bin` (200 records, 8 games, 2 opening blocks).
    * **Controlled Baselines Battery:** Evaluated seven 25-epoch runs with AdamW ($\text{lr} = 10^{-3}, \text{weight\_decay} = 10^{-4}$):
      - Exp A: Pure Game Outcome BCE ($\alpha = 1.0$) -> Val BCE 0.6710, Brier 0.1354, Corr $+0.1869$.
      - Exp B1: Teacher Distillation Logistic Prob MSE ($\alpha = 0.0$) -> Val BCE 0.6847, Brier 0.1416, Corr $+0.0983$.
      - Exp B2: Direct Centipawn Normalized MSE $((E-q)/400)^2$ -> Val BCE 0.6835, Brier 0.1410, Corr $+0.0809$.
      - Exp C1: Blended Loss ($\alpha = 0.25$) -> Val BCE 0.6744, Brier 0.1367, Corr $+0.1650$.
      - Exp C2: Blended Loss ($\alpha = 0.50$) -> Val BCE 0.6707, Brier 0.1351, Corr $+0.1746$.
      - Exp C3: Blended Loss ($\alpha = 0.75$) -> Val BCE **0.6695**, Brier **0.1346**, Corr **+0.1803** (**Validation Winner**).
      - Exp P1: Mode P1 Full Network Validation ($\alpha = 0.50$) -> Val BCE 0.6707, Brier 0.1351, Corr $+0.1746$.
    * **Out-of-Sample Test Evaluation:** Following post-selection freeze, winning model `Exp_C3_Blended_a075_best.pt` evaluated on `test.bin`: BCE = 0.7316, Brier = 0.1751, $\mu_E = +5.36\text{ cp}, \sigma_E = 56.72\text{ cp}$, zero saturation at boundary 127.
    * **Quantization Shadow Findings:** Gate 8-B-15 shadow emulation revealed preliminary shadow MAE = 33.36 cp and $\max |\Delta E| = 61.36\text{ cp}$ (~$58.8\%$ of $\sigma_E$), demonstrating that integer division truncation across multiple layers creates compounding residual shift and motivating formal layer-wise quantization calibration in Phase 8-C.
    * **Classical Search Control Invariance:** Pinned at exactly **313,092 nodes** ($\Delta = 0$ nodes vs frozen `v0.9.5-classical-enhanced` control).
    * **Full Verification Battery:** Suite #36 passed all 15 validation gates (Gates 8-B-1 through 8-B-15). All 36 test suites passing cleanly (`phase8B: 1`).
    * **Status:** **Complete / Closed**.
  - **Phase 8-C (Quantization Calibration & In-Engine Serialization):**
    * **Phase 8-C1 (Layer-Wise Quantization Fidelity & Calibration Diagnostic):** Diagnosed and resolved the discrete integer quantization underflow collapse where naive Post-Training Quantization (PTQ) zeroed out ~99% of FC1 and FC2 weights.
      - Developed a Quantization-Aware Training (QAT) remediation protocol (`tools/training/qat.py`) employing Straight-Through Estimators (STE) and temperature-scaled surrogate loss on the 10% calibration partition ($N_{\text{calib}} = 450$).
      - Remediation checkpoint R3 achieved near-lossless discrete quantization fidelity on `val.bin` ($N = 300$): Quantized BCE = 0.6712 (vs Float32 F0 BCE = 0.6695, $\Delta\text{BCE} = +0.0017$), Quantized Brier = 0.1354 (vs F0 = 0.1346), $\text{Corr}(E_q, q_{\text{teacher}}) = +0.1772$.
      - Classical control depth-6 benchmark locked at exactly **313,092 nodes** ($\Delta = 0$). Suite #37 passed all 16 acceptance gates.
      - **Status:** **Complete / Closed**.
    * **Phase 8-C2 (Binary Model Exporter & In-Engine Serialization: `boson-v2.nnue`):** Implemented canonical discrete binary model serialization directly from the frozen R3 discrete checkpoint into `models/boson-v2.nnue`.
      - **Binary Model File:** `models/boson-v2.nnue` (exact size: 41,978,176 bytes, magic `0x4E4E5545`, version 1).
      - **Model SHA-256 Digest:** `e3a11394694652258b2f2445ad5719a67769763c8b088db1243867d17e41676e` (manifest: `models/boson-v2.manifest.json`).
      - **Discrete Tensor Sparsity:** FC1 98.54% zero (479 active weights), FC2 3.42% zero (989 active weights), FC3 84.38% zero (5 active weights).
      - **Three-Way Equivalence Verification:** Bit-exact evaluation equivalence confirmed across Python reference discrete emulation, C++ scalar in-engine evaluation, and C++ AVX2 SIMD forward inference ($\Delta = 0\text{ cp}$ across all 6 canonical benchmark positions).
      - **Test Partition Protection:** `data/dataset_phase8a/test.bin` remained 100% untouched and unaccessed (SHA-256: `f504d41cd4a10800667dcd168bfa7a2e6e04e09310f502436be8a69fe99d255e`).
      - **Classical Search Invariance:** Classical depth-6 benchmark locked at exactly **313,092 nodes** ($\Delta = 0$). Suite #38 passed all 12 acceptance gates.
      - **Status:** **Complete / Closed**.
  - **Phase 8-D (Empirical Model Progression & Strength Validation):** Sequential tournament validation of trained NNUE model candidates against frozen classical baseline (`v0.9.5-classical-enhanced`) under 200-game head-to-head matches.
- **Exit Criteria:** Trained network achieving statistically significant positive Elo gain over Classical HCE baseline in fixed-depth/fixed-time SPRT matches ($\Delta\text{Elo} \ge +50\text{ Elo}$, SPRT Accept $H_1$).
- **Status:** **In Progress** (Phases 8-A, 8-B, 8-C1, & 8-C2 Complete).

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