# Correction History — Evaluation Bias Correction Architecture

## 1. Theoretical Foundation: Move Histories vs. Correction History

In computer chess search pipelines, heuristic tables fall into two fundamentally distinct categories:

1. **Move Ordering Heuristics (Branch Selection):**
   * *Examples:* Transposition Table best move, Killer Moves, Counter-Move History (CMH), Butterfly History, Continuation History.
   * *Purpose:* Rank candidate moves at a node to maximize the probability of exploring the refutation first, achieving optimal alpha-beta branch pruning ($O(b^{d/2})$).
   * *Scope:* Operates strictly on discrete moves. Has zero effect on positional evaluation scores.

2. **Correction History (Evaluation Calibration):**
   * *Examples:* Pawn Structure Correction History, Material Imbalance Correction History.
   * *Purpose:* Dynamically calibrate the static evaluation function to match minimax search reality. Handcrafted evaluation functions (and NNUE nets) exhibit systematic blind spots—such as overvaluing certain blocked pawn chains, misjudging piece activity with specific pawn topologies, or underestimating color-bound weaknesses. Correction History observes the persistent error between static evaluation and nominal search score, feeding an offset back into the evaluator.
   * *Scope:* Operates strictly on static positions, altering the leaf and stand-pat evaluation values.

```
+-------------------------------------------------------------+
|                      Alpha-Beta Search                      |
|                                                             |
|   +-----------------------+     +-----------------------+   |
|   | Move Orderer          |     | Evaluator             |   |
|   | - Killers / CMH       |     | - Material & PST      |   |
|   | - Continuation Hist   |     | - Tapered King Safety |   |
|   +-----------------------+     +-----------+-----------+   |
|               ^                             |               |
|               |                             v (Static Eval) |
|               |                 +-----------+-----------+   |
|               |                 | Correction History    |   |
|               |                 | Table (Pawn Zobrist)  |   |
|               |                 +-----------+-----------+   |
|               |                             |               |
|               +-----------------------------+               |
|            Calibrated Minimax Score Convergence             |
+-------------------------------------------------------------+
```

---

## 2. Subsystem Ownership & Decoupled Architectural Contract

### Subsystem Ownership Boundary:
* **Owner Subsystem:** `Evaluation` (`Evaluator`).
* **Table Instance:** `Evaluator::s_corrTable` (`CorrectionHistoryTable`).
* **Updater:** `Search::negamax()`.
* **Consumer:** `Evaluator::evaluate()`.

### Rationale:
Correction History belongs conceptually and structurally to the evaluation layer because its output directly alters the composite evaluation score. By encapsulating `CorrectionHistoryTable` within the `Evaluation` subsystem, the evaluator remains autonomous: `Evaluator::evaluate(pos)` produces a fully calibrated evaluation score without the caller needing to manually coordinate correction tables.

### Read-Only Consumer Invariant:
When `Evaluator::evaluate(const Position& pos)` probes Correction History via `s_corrTable.probe(pos)`, the operation is strictly read-only (`const Position&`). Probing performs bitwise scans over pawn bitboards without mutating board state, undo stacks, or Zobrist hashes.

---

## 3. Feature-Based Indexing & Gravity Decay Mathematics

### 3.1 Feature Topology: Pawn Structure Zobrist Hashing
Rather than indexing by individual move transitions, Correction History is keyed by macroscopic position-level features that dictate long-term strategic balance:

$$\text{PawnKey} = \bigoplus_{sq \in \text{WhitePawns}} \text{Zobrist}[P_{\text{WP}}][sq] \oplus \bigoplus_{sq \in \text{BlackPawns}} \text{Zobrist}[P_{\text{BP}}][sq]$$

$$\text{BucketIndex} = \text{PawnKey} \pmod{16384}$$

$$\text{Offset} = \text{Table}[\text{Color}][\text{BucketIndex}]$$

### Invariance Properties:
1. **Piece Movement Invariance:** Non-pawn piece maneuvers (Knights, Bishops, Rooks, Queens, Kings) leave the pawn bitboards untouched. Consequently, all positional permutations sharing the exact same pawn skeleton share the same correction memory bucket.
2. **Pawn Movement Sensitivity:** Any pawn push, capture, or promotion instantly transitions the position into a new topological bucket, isolating pawn-structure-specific biases.

### 3.2 Depth-Scaled Gravity Decay Update Rule
When a search branch resolves at depth $d \ge 2$ in a stable non-check, non-mate node ($|\text{bestScore}| < \text{MATE} - 100$):

$$\text{Discrepancy} = \text{searchScore} - \text{staticEval}$$

Tactical noise and sacrificial spikes ($|\text{Discrepancy}| > 1200\text{ cp}$) are rejected outright. For valid positional discrepancies:

$$\text{Weight} = \min(d^2, 256)$$

$$\text{NewVal} = \text{CurrentVal} + \frac{\text{Discrepancy} \times \text{Weight} - \text{CurrentVal}}{1024}$$

$$\text{NewVal} \leftarrow \text{clamp}(\text{NewVal}, -1024, 1024)$$

This exponential moving average ensures that deeper, more stable search results exert greater influence, while preventing single-node statistical outliers from destabilizing the evaluation baseline.

---

## 4. Interaction with Other Heuristics & NNUE Roadmap

1. **Aspiration Window Stability:** By correcting systematic evaluation biases, the root evaluation remains centered within the narrow initial aspiration window ($\pm 30\text{ cp}$), drastically minimizing fail-high / fail-low re-search cycles.
2. **Null Move Pruning (NMP) Safety:** Accurate static evaluation ensures that NMP is not triggered on positions that are falsely evaluated as winning when they are strategically compromised.
3. **Quiescence Search Acceleration:** Stand-pat evaluation cutoffs in quiescence search become sharper, avoiding wasteful tactical extensions in positions that are stably equal.
4. **NNUE Architecture Transition:** The feature-indexed Correction History table operates orthogonally to the evaluation core. When Boson transitions from handcrafted PST evaluation to an embedded NNUE inference engine, `CorrectionHistoryTable` requires zero refactoring—it will seamlessly calibrate NNUE network output against tree search realities.