# Architectural Decision Records (ADR)

This document codifies the foundational architectural decisions of the Boson chess engine (ADR-001 through ADR-010). Each record establishes the engineering context, architectural decision, rationale, and rejected alternatives.

---

## ADR-001: Transactional In-Place Mutation with Stack UndoState vs Copy-on-Write

* **Status:** Accepted
* **Date:** 2026-06-10
* **Scope:** Board State & Move Execution Subsystem

### Context
Chess engines execute between $10^6$ and $10^8$ tree node transitions per second during lookahead search. Maintaining board consistency, castling permissions, en passant flags, halfmove clocks, and incremental Zobrist hashes across deep recursive stacks is performance-critical.

### Decision
Implement in-place state mutation on a single stack-allocated `Position` object paired with a compact, trivially destructible `UndoState` struct ($\le 32$ bytes). State changes are committed via `MoveExecutor::makeMove` and reversed via `MoveExecutor::undoMove`.

### Rationale
1. **Zero Dynamic Allocation:** Eliminates heap allocator churn, cache line thrashing, and memory fragmentation entirely in the hot search loop.
2. **L1/L2 Cache Spatial Locality:** The active `Position` and recursive `UndoState` records reside in contiguous stack memory, maximizing CPU cache residency.
3. **Deterministic Rollback:** Restores board occupancy, 64-bit Zobrist hash keys, and irreversible state counters in $O(1)$ constant time.
4. **Value Semantics:** Preserves copyability and test isolation without coupling the engine to reference counting or pointer indirection.

### Rejected Alternatives
* **Copy-on-Write Position Allocations:** Passing board copies by value or clone on each ply incurs unacceptable memory bandwidth consumption ($>10\times$ node throughput degradation).
* **Immutable Pure Functional State:** Allocating new board instances at every tree edge violates the zero-allocation performance guarantee.

---

## ADR-002: Modular FEN Parsing Layered Separately from Semantic Validation

* **Status:** Accepted
* **Date:** 2026-06-12
* **Scope:** FEN Parser & Domain Primitives

### Context
Forsyth-Edwards Notation (FEN) strings arrive from external UCI interfaces, test harnesses, and opening books. Textual parsing must handle corrupted formatting, out-of-range piece coordinates, and malformed castling tokens without corrupting internal invariants.

### Decision
Decouple FEN processing into a pure six-stage text parser (`FenParser`) that returns `std::optional<Position>`, completely separate from internal board construction and subsequent semantic verification (`VerificationHarness`).

### Rationale
1. **Single Responsibility Principle:** `FenParser` handles textual tokenization and field syntax; `Position` represents domain invariants; `VerificationHarness` asserts legal chess reality.
2. **Zero-Exception Error Handling:** Malformed FEN inputs fail fast by returning `std::nullopt`, avoiding C++ exception handling overhead and ensuring predictable error propagation.
3. **Auditability:** Each parser stage (piece placement, active color, castling rights, en passant target, halfmove clock, fullmove number) is independently unit-testable.

### Rejected Alternatives
* **Monolithic Parsing inside Position Constructor:** Throwing exceptions or constructing partially initialized `Position` objects violates RAII and creates brittle state dependencies.
* **Regular Expression (Regex) Parsing:** Standard library regex engines introduce significant parsing latency and unpredictable memory allocation behaviors.

---

## ADR-003: Read-Only Observer MoveGenerator vs Transactional MoveExecutor

* **Status:** Accepted
* **Date:** 2026-06-20
* **Scope:** Move Generation & Board Architecture

### Context
Move generation requires scanning sliding piece rays, leaper attack tables, and pawn advance masks. Merging generation and mutation into a single interface frequently produces state synchronization bugs and race conditions in concurrent searches.

### Decision
Separate responsibilities cleanly:
* `MoveGenerator`: Pure read-only observer that takes `const Position&` and generates candidates into a pre-allocated stack `MoveList`.
* `MoveExecutor`: Dedicated transactional state machine that mutates `Position` and restores state via `UndoState`.

### Rationale
1. **Const-Correctness:** The compiler strictly prevents `MoveGenerator` from modifying board state, attack masks, or hash keys.
2. **Thread Safety:** Multiple worker threads (or parallel search split points) can inspect the same `Position` instance concurrently without synchronization locks.
3. **Legality vs Pseudo-Legality:** Allows efficient generation of pseudo-legal moves for quiescence filtering and legal move filtering for root search.

### Rejected Alternatives
* **Self-Executing Move Objects:** Embedding `make()` and `undo()` inside the `Move` class bloats move representations beyond 32 bits and creates cyclical dependencies with `Position`.
* **State-Mutating Move Generator:** Generating moves while modifying board state risks leaving the board in an invalid state upon search cutoffs or timeouts.

---

## ADR-004: Decoupled Standalone SEE Subsystem vs Embedded Evaluator Functions

* **Status:** Accepted
* **Date:** 2026-07-05
* **Scope:** Static Exchange Evaluation & Search Heuristics

### Context
Static Exchange Evaluation (SEE) simulates tactical trades on a target square to determine whether a capture is winning, equal, or losing. It is required during move ordering (to score captures) and quiescence search (to prune negative exchanges).

### Decision
Isolate SEE into a standalone pure function subsystem (`boson::SEE`) in `engine/include/search/see/SEE.hpp`, accepting `const Position& pos, Square from, Square to`.

### Rationale
1. **Zero State Mutation:** SEE operates strictly on copied 64-bit occupancy bitboards and precalculated attacker bitmasks without invoking `makeMove` or modifying `Position`.
2. **Independent Replaceability:** SEE piece values and least-valuable-attacker policies can be tuned or replaced without affecting the primary `Evaluator` scoring weights.
3. **Subsystem Decoupling:** Move ordering and search pruners query SEE without depending on full evaluation knowledge (PSTs, pawn structures, king safety).

### Rejected Alternatives
* **Embedding SEE into Evaluator:** Coupes tactical swap simulations with positional evaluation terms, violating modular replacement principles.
* **Full Recursive Search Extensions:** Simulating captures via actual recursive `makeMove`/`undoMove` calls causes severe node count explosion.

---

## ADR-005: Multi-Tier Heuristic Move Ordering Stratification

* **Status:** Accepted
* **Date:** 2026-07-28
* **Scope:** Move Ordering & Alpha-Beta Cutoff Optimization

### Context
Alpha-Beta pruning efficiency depends on finding the best move first. In optimal ordering, tree search complexity approaches $O(b^{d/2})$ rather than $O(b^d)$.

### Decision
Stratify move sorting into strict, discrete priority tiers:
1. Transposition Table Move ($10,000,000$)
2. Winning & Equal Captures (SEE $\ge 0$ + MVV-LVA score)
3. Pawn Promotions
4. Killer Moves (Slot 1: $800,000$, Slot 2: $700,000$)
5. Continuation History Heuristic
6. Counter-Move History (CMH)
7. Global History Heuristic
8. Losing Captures (SEE $< 0$)
9. Unpromising Quiet Moves

### Rationale
1. **Transposition Dominance:** TT hits from earlier iterations or shallower searches provide the best move $>85\%$ of the time.
2. **Tactical Safety First:** Separating winning captures from losing captures avoids searching suicidal exchanges before quiet killer moves.
3. **Contextual History Priority:** Moves proven successful in the immediate tactical context (counter-moves, continuation) are searched before global history fallbacks.

### Rejected Alternatives
* **Pure MVV-LVA Capture Sorting:** Fails to account for defended pieces, ordering losing captures (e.g. $Q \times P$ defended by $P$) ahead of quiet killers.
* **Unstratified Single-Score Heuristics:** Mixing disparate signals into a single unscaled integer introduces priority inversions and brittle tuning interactions.

---

## ADR-006: Stand-Pat Quiescence Search with Zero-Window Pruning

* **Status:** Accepted
* **Date:** 2026-07-28
* **Scope:** Horizon Effect Mitigation & Tactical Stability

### Context
Evaluating positions at an arbitrary fixed depth exposes search to the horizon effect—overlooking tactical blunders (such as an unrecaptured Queen) that occur on the next ply.

### Decision
Implement a specialized quiescence search (`Search::quiescence`) that evaluates only non-quiet moves (captures and promotions) to tactical quietude, utilizing a "stand-pat" lower bound and delta pruning.

### Rationale
1. **Stand-Pat Cutoff:** If the static evaluation of the current position already exceeds $\beta$, the side to move can choose not to capture, triggering an immediate cutoff.
2. **Delta Pruning:** Captures that fail to raise the score above $\alpha - \text{margin}$ even when capturing the most valuable piece are pruned immediately.
3. **Tactical Boundedness:** Excludes non-forcing quiet moves, preventing exponential subtree expansion while ensuring evaluation accuracy at the search frontier.

### Rejected Alternatives
* **Searching Quiet Checks in Quiescence:** Expands the quiescence search tree exponentially, dropping node throughput by $>70\%$.
* **Fixed-Depth Horizon Extension:** Arbitrarily extending all nodes near the horizon increases branching factor without resolving non-deterministic tactics.

---

## ADR-007: Logarithmic LMR with Zero-Window Scout and Full-Depth Re-search

* **Status:** Accepted
* **Date:** 2026-08-15
* **Scope:** Selective Search & Late Move Reductions

### Context
In a properly sorted move list, late quiet moves rarely exceed $\alpha$ or refute the principal variation. Searching every late quiet move to full nominal depth wastes computational budget on redundant branches.

### Decision
Apply Late Move Reductions (LMR) using a precomputed 2D logarithmic reduction table:
$$R(d, m) = 0.5 + \frac{\ln(d) \ln(m)}{1.95}$$
Late quiet moves ($m \ge 4$, $d \ge 3$) are searched with reduction $R$ using a null-window scout search ($[- \alpha - 1, -\alpha]$). If the reduced search exceeds $\alpha$, a full-depth re-search is triggered immediately.

### Rationale
1. **Logarithmic Decay:** Reflects the diminishing probability of move superiority as move index $m$ and depth $d$ increase.
2. **Strict Exemption Guards:** Tactical moves (captures, promotions), checks, check evasions, killer moves, and king-zone attacks receive reduced or zero reductions.
3. **Fail-Soft Safety:** The zero-window scout verifies failure; triggering full-depth unreduced re-search guarantees tactical completeness and prevents search degradation.

### Rejected Alternatives
* **Linear Reduction Formulas ($R = d/c$):** Over-reduces shallow moves while under-reducing deep late moves, harming tactical solving rates.
* **Pruning without Re-search:** Permanently discarding reduced moves that fail high causes catastrophic blind spots on tactical sacrifices.

---

## ADR-008: Non-Pawn Material Zugzwang Safeguards in NMP

* **Status:** Accepted
* **Date:** 2026-08-20
* **Scope:** Null Move Pruning & Endgame Stability

### Context
Null Move Pruning (NMP) passes the turn to the opponent ($R=2$ reduction). If the opponent cannot beat $\beta$ even with two consecutive moves, the position is declared an overwhelming cutoff. However, in zugzwang positions, passing the turn is an advantage, leading to false cutoffs.

### Decision
Enforce strict NMP entry criteria:
1. `!inCheck`: Never execute null moves when the King is under attack.
2. `allowNull`: Child searches inherit `allowNull = false` to prevent consecutive null moves.
3. `pos.hasNonPawnMaterial(pos.getSideToMove())`: Disable NMP entirely if the moving side has only pawns and a king.

### Rationale
1. **Zugzwang Immunity:** True zugzwang occurs almost exclusively in pawn endgames. Restricting NMP to positions containing at least one minor or major piece prevents catastrophic false cutoffs.
2. **$O(1)$ Verification:** Material check executes via a single bitwise OR across knight, bishop, rook, and queen bitboards.
3. **Substantial Tree Pruning:** Yields $>40\%$ tree reduction in tactical and middlegame positions without endgame degradation.

### Rejected Alternatives
* **Unconditional NMP:** Blunders forced wins into losses in technical King+Pawn endgames.
* **Complex Endgame Detectors:** High runtime verification cost outweighs NMP node savings.

---

## ADR-009: Layered Temporal Experience (Global History -> CMH -> Multi-Ply Continuation)

* **Status:** Accepted
* **Date:** 2026-09-02
* **Scope:** Move Ordering & Contextual History Heuristics

### Context
Global history tables track move cutoff frequency across the entire game, but lack context regarding preceding moves or tactical piece coordination.

### Decision
Layer search experience heuristics into three complementary structures:
1. **Global History Table:** Butterfly board matrix $[12][64]$ accumulating quiet cutoff frequency scaled by saturating gravity ($D = 16384$).
2. **Counter-Move History (CMH):** L1-friendly table $[12][64]$ storing the single refutation move to the opponent's previous move.
3. **Continuation History (ContHist):** Multi-ply matrix $[12][64][64]$ tracking move success in the context of the engine's own previous move from 2 plies ago.

### Rationale
1. **Contextual Specificity:** Continuation history identifies natural piece redeployments and tactical continuations; CMH catches immediate refutations.
2. **Cache Efficiency:** CMH and Continuation tables fit within CPU L1/L2 cache lines, enabling single-cycle lookups during move scoring.
3. **Graceful Degradation:** When local contextual history is cold (zero hits), the orderer falls back seamlessly to global history.

### Rejected Alternatives
* **Global History Alone:** Misses obvious refutations dependent on the opponent's immediate previous move.
* **Deep 4-Ply/6-Ply Continuation Tensors:** Causes memory footprint inflation ($>50\text{ MB}$), cache eviction storms, and severe table sparsity.

---

## ADR-010: Evaluation Bias Correction (CorrHist) Owned by Evaluation with Pawn Topology Keying

* **Status:** Accepted
* **Date:** 2026-09-05
* **Scope:** Positional Evaluation & Self-Correcting Search

### Context
Static evaluation functions (HCE or NNUE) suffer from structural blind spots, systematically over- or under-valuing certain pawn structures or positional themes relative to full-depth search scores.

### Decision
Implement Correction History (`CorrectionHistoryTable`) owned strictly by the evaluation subsystem:
* **Keying:** Keyed by `(Color, PawnKey % 16384)`, where the pawn Zobrist key depends exclusively on White and Black pawn placements.
* **Read-Only Invariant:** `Evaluator::evaluate(pos)` queries `probe(pos)` as a read-only const operation, adding a bounded offset ($\pm 1024$ cp).
* **Gravity Updates:** Updated during search on stable non-check, non-mate nodes ($d \ge 2$) using depth-scaled gravity decay ($w = \min(d^2, 256)$).

### Rationale
1. **Structural Stability:** Pawn structures change slowly throughout a game. Keying by pawn skeleton preserves learned bias adjustments across transient piece maneuvers.
2. **Subsystem Isolation:** Owned by `evaluation`, preserving the unidirectional dependency where evaluation scores positions and search consumes scores.
3. **NNUE Compatibility:** Correction history directly translates to NNUE evaluation, correcting network blind spots and endgame horizon shifts.

### Rejected Alternatives
* **Keying by Full 64-bit Zobrist Hash:** Table sparsity is too high; identical pawn structures with minor piece maneuvers would never share learned bias.
* **Search-Owned Correction State:** Placing evaluation correction inside `Search` violates subsystem boundaries and prevents standalone evaluation benchmarking.
