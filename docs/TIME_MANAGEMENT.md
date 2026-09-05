# Time Management and Search Control Architecture

## 1. Architecture Hierarchy and Subsystem Decoupling

Boson organizes search execution, time allocation, and cancellation into three decoupled architectural tiers:

```
+-------------------------------------------------------------------+
|                           TimeManager                             |
|  - Ingestion: UCI SearchLimits (wtime, btime, winc, binc, movetime)|
|  - Output: Soft Limit (iteration boundary) & Hard Limit (panic)   |
|  - Pure allocation mathematics; completely search-agnostic        |
+-------------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------------+
|                        SearchController                           |
|  - Search session lifetime coordinator                            |
|  - StopToken: Atomic boolean cancellation flag                    |
|  - Telemetry centralization: SearchStatistics                     |
|  - Periodicity enforcement (checkTime caller)                     |
+-------------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------------+
|                          Search Engine                            |
|  - Iterative Deepening loop (Search::runSearch)                   |
|  - Alpha-Beta tree traversal (Search::negamax)                    |
|  - Quiescence tactical horizon (Search::quiescence)               |
+-------------------------------------------------------------------+
```

### Architectural Guarantees
1. **Separation of Concerns**: `TimeManager` never inspects board geometry, plies, or alpha-beta values. It calculates time budgets purely from clock state and increment.
2. **Controller Decoupling**: `SearchController` provides the thread-safe stop token, elapsed clock tracking, and telemetry aggregation. Search queries `controller.shouldStop()` without directly polling system clocks on every node.

---

## 2. Mathematical Formulation of Soft vs. Hard Limits

For game clock configurations, time allocation distinguishes between the **Soft Limit** (the threshold for launching an additional full iteration) and the **Hard Limit** (the non-negotiable abort ceiling for an in-flight search tree).

### 2.1 Fixed Move Time (`movetime`)
When the GUI issues `go movetime <ms>`:
$$\text{SoftLimit} = \text{movetime}$$
$$\text{HardLimit} = \text{movetime}$$

### 2.2 Classical & Incremental Time Control
Given remaining time pool $T_{\text{avail}}$ and increment $T_{\text{inc}}$ for the side to move:

1. **Soft Limit (Target Allocation)**:
   $$\text{SoftLimit} = \left\lfloor \frac{T_{\text{avail}}}{20} \right\rfloor + \left\lfloor \frac{T_{\text{inc}}}{2} \right\rfloor$$
   - Allocates $5\%$ of the base bank per move plus half the increment to preserve a steady time buffer across typical 40-move phases.
   - Clamped to $[1, T_{\text{avail}} - 50\text{ ms}]$ to avoid immediate flag fall.

2. **Hard Limit (Panic Ceiling)**:
   $$\text{HardLimit} = \left\lfloor \frac{T_{\text{avail}}}{4} \right\rfloor$$
   - Bounds the maximum permissible time consumption for complex positions requiring unexpected aspiration re-searches.
   - Clamped to $T_{\text{avail}} - 20\text{ ms}$ safety margin.

---

## 3. Periodic Node-Check Polling Mechanism

Invoking `std::chrono::high_resolution_clock::now()` incurs a kernel/hardware timer interrogation penalty of approximately $15 \dots 30\text{ ns}$. In an engine traversing $10^6 \dots 10^7\text{ nodes/sec}$, polling the clock on every node would impose an intolerable $15\% \dots 30\%$ throughput degradation.

Boson amortizes clock overhead using a bitmask check:
```cpp
if (stats.nodes % NODE_CHECK_PERIOD == 0) {
    controller.checkTime();
    if (controller.getLimits().nodes > 0 && stats.nodes >= static_cast<uint64_t>(controller.getLimits().nodes)) {
        controller.requestStop(StopReason::NodesLimit);
    }
}
```
Where `NODE_CHECK_PERIOD = 2048`. At typical search speeds of $1.5\text{ MNPS}$, time checks occur approximately every $1.3\text{ ms}$, ensuring near-instantaneous response while reducing clock polling overhead to $< 0.05\%$.

---

## 4. Transactional Unwind Safety Guarantee

A fundamental engine invariant is that interrupting a search must never corrupt the board state or leak unmade moves.

In both `Search::negamax` and `Search::quiescence`:
```cpp
MoveExecutor::makeMove(pos, legalMoves[i], undo);
movesSearched++;

score = -negamax(pos, ...);

// CRITICAL: Unwind execution is unconditional
MoveExecutor::undoMove(pos, legalMoves[i], undo);

if (controller.shouldStop()) return 0;
```

### Invariant Rules
1. **Unconditional Reversion**: `MoveExecutor::undoMove()` is invoked immediately upon returning from the child subtree, before any `shouldStop()` check.
2. **Null-Move Symmetry**: Null-move state modifications (`setSideToMove`, `setEnPassantSquare`) are reverted immediately prior to returning.
3. **Bit-for-Bit Identity**: Across all aborted, interrupted, or completed searches, the root `Position` bitboards, castling rights, en-passant square, and Zobrist hash key remain identical before and after search execution.

---

## 5. Fallback to Completed Iteration State

When time expires during iteration $d$:
1. The search tree unwinds rapidly via `if (controller.shouldStop()) return 0;`.
2. The root iterative loop checks `controller.shouldStop()` immediately after the root `negamax` call:
   ```cpp
   if (controller.shouldStop()) {
       break;
   }
   lastScore = score;
   stablePv  = iterationPv;
   stats.completedDepth = d;
   ```
3. Because the break occurs before assignment, the incomplete score, corrupted PV, and partial depth metrics are discarded.
4. `Search::runSearch()` returns `lastScore`, and `SearchController::getStats().pvLine` retains the intact line from iteration $d - 1$.
