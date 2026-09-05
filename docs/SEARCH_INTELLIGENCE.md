# Search Intelligence: Iterative Deepening and Principal Variation Architecture

## 1. Iterative Deepening Loop Mechanics

Alpha-beta pruning performance is overwhelmingly dominated by move ordering. If the best move is examined first at every node, the effective branching factor drops from $b \approx 35$ to $\sqrt{b} \approx 6$, allowing search depth to nearly double for the same computational budget.

Rather than searching directly to nominal depth $D$, Boson employs **Iterative Deepening (ID)**:

```
for (int d = 1; d <= maxDepth; ++d) {
    // 1. Time management guard (Soft time limit)
    if (elapsedTime >= softLimit) break;

    // 2. Root PV lock & Transposition Table seeding
    if (d > 1 && stablePv.count > 0) {
        s_tt.store(pos.getHashKey(), scoreToTT(lastScore, 0), stablePv.moves[0], d + 2, TTNodeType::Exact, 0);
    }

    // 3. Search iteration d (with Aspiration Windows when d >= 5)
    score = searchIteration(pos, d, ...);

    // 4. Abort guard: If hard limit or stop token fired mid-iteration, drop unverified results
    if (shouldStop()) break;

    // 5. Commit verified iteration state
    lastScore = score;
    stablePv  = iterationPv;
    stats.completedDepth = d;
}
```

### Cache-Warming & Ordering Efficiency
- **TT Seeding**: The Transposition Table (TT) caches the best moves, subtree bounds, and evaluations from depth $d - 1$. When iteration $d$ commences, the root move and critical interior branch points immediately hit valid entries, placing the previous iteration's best move in the highest ordering tier (`SCORE_TT = 100,000`).
- **Overhead Amortization**: In a tree with branching factor $b \ge 3$, the sum of nodes from depths $1$ through $d-1$ is given by:
  $$\sum_{i=1}^{d-1} b^i \approx \frac{b^d - 1}{b - 1} \ll b^d$$
  For $b \approx 6$, the total overhead of all prior iterations combined is less than $20\%$ of depth $d$. The dramatic move ordering improvements from cached entries more than offset this overhead, yielding a net reduction in total search time.

---

## 2. Triangular Principal Variation (PV) Architecture

Boson collects the Principal Variation (PV)—the complete sequence of expected best play from the root position to the nominal depth horizon—using a stack-based triangular table model (`PVLine`).

### 2.1 Triangular Table Layout
For maximum search ply `MAX_PLY = 64`:

```
ply 0:  [ m0 ] [ m1 ] [ m2 ] [ m3 ] ... [ md ]   <-- Full Root PV
ply 1:         [ m1 ] [ m2 ] [ m3 ] ... [ md ]
ply 2:                [ m2 ] [ m3 ] ... [ md ]
...
ply d:                                  [ md ]   <-- Leaf Move
```

Instead of managing a monolithic global 2D matrix on the heap, each recursive stack frame of `Search::negamax` maintains an in-frame stack buffer:
```cpp
PVLine childPv;
```

### 2.2 Propagation Invariants
When candidate move $m_i$ at ply $p$ produces a score exceeding $\alpha$ (`score > alpha`):

1. **Root & Interior Assignment**:
   The current move is placed at index 0 of the local line:
   $$\text{pv.moves}[0] = m_i$$
2. **Triangular Propagation**:
   The successor sequence from `childPv` is copied into `pv.moves` offset by 1:
   $$\text{pv.moves}[j + 1] = \text{childPv.moves}[j] \quad (\forall j \in [0, \text{childPv.count}-1])$$
   $$\text{pv.count} = \text{childPv.count} + 1$$
3. **Leaf Initialization**:
   When entering quiescence or reaching terminal states (`legalMoves.empty()`), `pv.count` resets to $0$.
4. **Legality Invariant**:
   Every move in `stablePv` forms a valid, contiguous, and legal chess move sequence playable directly on the root `Position`.

---

## 3. Strict Subsystem Boundaries

Boson enforces clean architectural boundaries between Search, Evaluation, and Time Management:

```
+-------------------------------------------------------------+
|                      SearchController                       |
|  - Stop token (atomic flag)                                 |
|  - Time allocation (TimeManager)                            |
|  - Telemetry counters & stats (SearchStatistics)            |
+-------------------------------------------------------------+
        |                                       |
        v                                       v
+------------------------+             +------------------------+
|      Search Engine     |             |      TimeManager       |
|  - negamax()           |             |  - Soft time limit     |
|  - quiescence()        |             |  - Hard time limit     |
|  - Iterative Deepening |             |  - Clock check period  |
+------------------------+             +------------------------+
        |                                       ^
        |                                       |
        v                                       v
+------------------------+             +------------------------+
|       MoveOrderer      |             |       Evaluator        |
|  - TT Move             |             |  - Static evaluation   |
|  - Captures / MVV-LVA  |             |  - Material & PSQT     |
|  - History / Killers   |             |  - Pure const inputs   |
+------------------------+             +------------------------+
```

1. **Search $\to$ TimeManager**:
   Search polls `controller.checkTime()` periodically every `NODE_CHECK_PERIOD = 2048` nodes. Search never computes clock formulas or manages system clocks directly.
2. **Search $\to$ Evaluation**:
   Evaluation is stateless and strictly read-only (`const Position&`). Search queries `evaluate(pos)` but `Evaluator` has zero knowledge of plies, moves, or search trees.
3. **MoveOrderer $\to$ MoveGenerator**:
   `MoveOrderer` does not generate moves; it consumes a mutable `MoveList&` produced by `MoveGenerator` and sorts elements in-place on stack buffers.

---

## 4. Search Roadmap (Modules 6.3 through 6.7)

The completion of Modules 6.1 and 6.2 establishes the iterative core. The remaining search milestones build directly upon this foundation:

| Module | Subsystem | Description & Objective |
|---|---|---|
| **6.3** | **Time Management** | Dynamic time allocation per move based on game phase, increment, move overhead, and sudden death. |
| **6.4** | **Aspiration Windows** | Narrow initial alpha-beta windows ($[\text{lastScore} - \Delta, \text{lastScore} + \Delta]$) to maximize cutoff rate at depths $\ge 5$, expanding exponentially on fail-high/fail-low. |
| **6.5** | **Static Exchange Evaluation (SEE)** | Fast ray-attack simulation to prune or demote tactically losing captures ($SEE < 0$) in ordering and quiescence. |
| **6.6** | **Null Move Pruning (NMP)** | Giving opponent a free pass ($R = 2$ or $3$ depth reduction) to detect positions so strong that even a null move cannot drop below beta (zugzwang protected). |
| **6.7** | **Late Move Reductions (LMR)** | Reducing nominal search depth on late, non-tactical quiet moves that lack killer or history support. |
