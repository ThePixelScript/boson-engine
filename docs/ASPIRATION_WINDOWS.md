# Aspiration Windows & Re-search Architecture

## 1. Architectural Purpose & Theoretical Foundation

In an Alpha-Beta search tree, the effective branching factor $b_{\text{eff}}$ depends directly on the width of the search window $(\alpha, \beta)$. Under the Knuth-Moore analysis:
- A full-window search $(-\infty, +\infty)$ requires examining $O(b^{d/2})$ nodes in the optimal move-ordering regime, but degrades towards $O(b^d)$ under imperfect ordering.
- A zero-window / null-window search $(a, a+1)$ converts the minimax search into a boolean test (proving whether a position is $> a$ or $\le a$), maximizing cutoffs.
- An **Aspiration Window** exploits the score stability of iterative deepening. Because the minimax score at depth $d$ closely approximates the score at depth $d-1$ ($S_d \approx S_{d-1}$), the search driver initializes a narrow window:
  $$[\alpha, \beta] = [\max(-\infty, S_{d-1} - \Delta), \min(+\infty, S_{d-1} + \Delta)]$$
  where $\Delta$ is an initial tolerance (default $\Delta = 30\text{ cp}$).

Narrowing the window forces sub-trees that deviate outside $[\alpha, \beta]$ to terminate early via beta-cutoffs or fail-low pruning, pruning extensive sub-branches and yielding a $10\%\text{ to }25\%$ reduction in total node consumption across middle-game iterations.

---

## 2. Decoupled Driver Architecture

Boson enforces strict separation between search coordination and recursive descent:

```
+-------------------------------------------------------------+
|              SearchController & Search Driver               |
|  - Manages Iterative Deepening loop (d = 1 .. maxDepth)     |
|  - Owns Aspiration Window loop: delta, alpha, beta, widening|
|  - Tracks aspiration telemetry: researches, fail-highs/lows |
|  - Handles time limits and clean abort fallback             |
+-------------------------------------------------------------+
                              |
                     Passes (pos, d, alpha, beta, ...)
                              v
+-------------------------------------------------------------+
|                     Search::negamax()                       |
|  - Pure Alpha-Beta recursive descent                        |
|  - Completely decoupled: ZERO awareness of aspiration       |
|    windows, delta values, or re-search iteration logic      |
|  - Returns raw subtree score evaluated against (alpha, beta)|
+-------------------------------------------------------------+
```

### Driver vs. Negamax Contract
1. **Iterative Deepening Entry Point**:
   - `Search::runSearch()` coordinates iterative deepening.
   - For $d = 1$: Searches with the unrestricted window $(-\infty, +\infty)$ to establish the initial baseline score.
   - For $d \ge 2$: Invokes `Search::searchWithAspiration(pos, d, lastScore, iterationPv)`.
2. **Recursive Descent Invariance**:
   - `Search::negamax()` accepts `int alpha` and `int beta` parameters. It enforces cutoffs whenever `score >= beta` or updates $\alpha$ when `score > alpha`.
   - `negamax()` does not widen bounds or trigger iterations; all re-search loops are contained within `searchWithAspiration()`.

---

## 3. Three-Outcome Classification & Widening Strategy

Given initial aspiration bounds $[\alpha, \beta] = [S_{d-1} - \Delta, S_{d-1} + \Delta]$, a search at depth $d$ yields one of three mutually exclusive outcomes:

```
                    Fail-Low            Inside Window           Fail-High
               (Score <= alpha)     (alpha < Score < beta)   (Score >= beta)
           <-----------------------|-----------------------|----------------------->
                                 alpha                   beta
```

### 3.1 Inside Window (Aspiration Success)
- **Condition**: $\alpha < \text{score} < \beta$.
- **Semantics**: The evaluation at depth $d$ remained stable within the expected boundary $\Delta$.
- **Action**: The iteration completes immediately in a single pass.
- **Telemetry**: `stats.aspirationSuccesses` is incremented; `stats.aspirationResearches` is untouched for this depth.

### 3.2 Fail-High (Optimistic Divergence)
- **Condition**: $\text{score} \ge \beta$.
- **Semantics**: The position contains a tactical breakthrough or advantageous line exceeding the optimistic upper bound. The score returned is a lower bound on the true score.
- **Action**:
  1. Record telemetry: `stats.failHighs++`, `stats.aspirationFailHigh++`, `stats.aspirationResearches++`.
  2. Widen the upper bound exponentially:
     $$\beta \leftarrow \min(+\infty, \beta + \Delta \times 2)$$
     $$\Delta \leftarrow \Delta \times 2$$
  3. If $\Delta > \Delta_{\max}$ (default $\Delta_{\max} = 400\text{ cp}$) or $\beta = +\infty$, fall back immediately to an unrestricted search window $(-\infty, +\infty)$.
  4. Re-search depth $d$ with the widened window.

### 3.3 Fail-Low (Pessimistic Divergence)
- **Condition**: $\text{score} \le \alpha$.
- **Semantics**: A defensive refutation, tactical penalty, or blunder line was revealed at depth $d$, dropping the score below the pessimistic bound. The score returned is an upper bound on the true score.
- **Action**:
  1. Record telemetry: `stats.failLows++`, `stats.aspirationFailLow++`, `stats.aspirationResearches++`.
  2. Widen the lower bound exponentially:
     $$\alpha \leftarrow \max(-\infty, \alpha - \Delta \times 2)$$
     $$\Delta \leftarrow \Delta \times 2$$
  3. If $\Delta > \Delta_{\max}$ ($400\text{ cp}$) or $\alpha = -\infty$, fall back immediately to $(-\infty, +\infty)$.
  4. Re-search depth $d$ with the widened window.

### 3.4 Fallback Threshold
When successive re-searches widen $\Delta$ beyond $400\text{ cp}$, the window is set to $[-\infty, +\infty]$. Searching $(-\infty, +\infty)$ guarantees an exact minimax score without further divergence risk.

---

## 4. Time Management & Transactional PV Protection

Aspiration re-searches increase search duration if multiple widenings occur. The driver enforces strict transactional integrity:

1. **Timer Polling in Re-search Loop**:
   - `SearchController::checkTime()` is checked before each re-search iteration.
   - If the hard time limit expires or an external stop is received, `searchWithAspiration()` exits immediately.
2. **Completed-Iteration Invariance**:
   - In `Search::runSearch()`, variables `lastScore`, `stablePv`, `stats.completedDepth`, and `stats.pvLine` are mutated **only after** an iteration finishes completely without abort:
     ```cpp
     score = searchWithAspiration(pos, d, lastScore, iterationPv);
     if (controller.shouldStop()) {
         break; // Discard partial/aborted iterationPv
     }
     lastScore = score;
     stablePv = iterationPv;
     stats.completedDepth = d;
     ```
   - If depth $d$ aborts during an aspiration re-search, the partial PV and score from the incomplete search are safely discarded. The engine reports the verified score and PV from depth $d - 1$.
3. **State Unwind Safety**:
   - In recursive descent (`negamax` and `quiescence`), moves are unwound via `MoveExecutor::undoMove()` prior to evaluating abort flags. Board state $P$ and Zobrist key $H$ remain mathematically invariant across mid-search aborts.

---

## 5. Telemetry & UCI Diagnostics

The search engine collects four distinct aspiration metrics per session within [`SearchStatistics`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/include/search/SearchStatistics.hpp):

| Field | Description |
| :--- | :--- |
| `aspirationSuccesses` | Total search depths where the score resolved within the current window. |
| `aspirationFailHigh` | Total re-searches triggered by score exceeding $\beta$. |
| `aspirationFailLow` | Total re-searches triggered by score falling below $\alpha$. |
| `aspirationResearches` | Total number of re-search passes across all iterations. |

Upon search termination, telemetry is logged:
```
--- Aspiration Optimization Analytics ---
  -> Total Window Successes : 9
  -> Window Fail Highs       : 1
  -> Window Fail Lows        : 0
  -> Total Re-Searches Hit   : 1
```
