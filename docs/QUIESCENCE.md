# Quiescence Search and Horizon Stability

## 1. The Horizon Effect and Nominal Depth Blindness

In minimax and alpha-beta search with a fixed depth bound $d$, tree traversal truncates strictly when $d \le 0$. If node evaluation occurs when the position is tactically volatile (e.g. midway through a piece trade, an in-flight queen capture, or an unaddressed check), the static evaluation function operates under the illusion of material inequality.

```
       [Nominal Depth d = 1]
                 |
         White plays QxR (+500 cp)
                 |
      ======================= <-- Nominal Depth Boundary (d = 0)
                 |
         Black plays BxQ (-900 cp)  <-- Blind to fixed-depth search without Quiescence
```

Without horizon resolution:
1. **Tactical Blindness**: The search evaluates intermediate states where material is hanging, overvaluing blunders or undervaluing sacrificial refutations.
2. **Horizon Pushing**: The engine executes worthless checks or delaying moves to push an inevitable tactical loss beyond nominal depth $d$.

To establish tactical quiescence and horizon stability, nominal search terminates when $d \le 0$ by delegating unconditionally to `Search::quiescence(pos, alpha, beta, ply)`. Positional `evaluate()` is never returned directly as a leaf score of `negamax()`.

---

## 2. The Stand-Pat Mathematical Contract

Quiescence search models a player's prerogative to "stand pat" (decline further tactical exchanges if the current static evaluation is already satisfactory), provided the side to move is not currently in check.

### Mathematical Formulation
Given current position $P$, search window $[\alpha, \beta]$, and static evaluation $S = \text{evaluate}(P)$:

1. **Check Condition**:
   - If `inCheck(P)` is true, standing pat is illegal. The king is under attack and an evasion move must be played. Full legal evasions are generated. If no legal evasions exist, checkmate is returned:
     $$\text{score} = -\text{MATE} + \text{ply}$$

2. **Beta Cutoff (Fail-High)**:
   - If not in check and $S \ge \beta$:
     $$\text{return } \beta$$
     The side to move already possesses a guaranteed positional advantage that exceeds the opponent's acceptable bound. The opponent will prune this branch; further capture searches are superfluous.

3. **Alpha Elevation**:
   - If $S > \alpha$:
     $$\alpha = S$$
     The stand-pat evaluation establishes a new lower bound for the current node. Any tactical capture that yields a score below $S$ is rejected in favor of declining the exchange.

---

## 3. Tactical Move Generation Constraints

Unlike nominal negamax nodes which generate all pseudo-legal or legal moves, quiescence search restricts generation to quiet the position with minimal branching factor:

| Node Condition | Generator Method | Generated Move Scope |
|---|---|---|
| `inCheck == true` | `MoveGenerator::generateLegalMoves` | All legal king evasions, blocks, and captures to escape check. |
| `inCheck == false` | `MoveGenerator::generateTacticalMoves` | Direct piece captures, en-passant captures, and pawn promotions ($8^{\text{th}}$ rank). |

### Invariants
1. **Quiet Move Exclusion**: Non-tactical pawn pushes, quiet piece maneuvers, and castling are excluded when `!inCheck`. Quiet moves consume search budget without addressing tactical volatility.
2. **Promotion Inclusion**: Pawn promotions fundamentally shift material balance and are generated as tactical moves.
3. **Termination Guarantee**: Traversal depth is hard-capped at $\text{ply} \ge \text{MAX\_PLY} - 1$ ($63$), preventing recursion runaway in deep tactical sequences. If `generateTacticalMoves` yields zero moves, the position is quiet, and $\alpha$ is returned.

---

## 4. Interaction with MoveOrderer

Tactical moves in quiescence search are ordered by `MoveOrderer::scoreAndSortTacticalMoves(pos, moves)`:

1. **Winning & Equal Captures ($\text{SEE} \ge 0$)**:
   - Scored with `SCORE_CAPTURES + MVV_LVA[victim][attacker]`.
   - Capturing high-value pieces with lower-value pieces ($P \times Q$, $N \times R$) is searched first.
2. **Promotions**:
   - Scored with `SCORE_PROMOTIONS` ($40,000$) with queen promotions prioritized over minor promotions.
3. **Losing Captures ($\text{SEE} < 0$)**:
   - Assigned `SCORE_LOSING_CAPTURES + SEE`, sorting least negative losses before catastrophic sacrifices.

Scoring operates in-place on stack buffers with selection sort, requiring zero heap allocations.

---

## 5. Pruning Roadmap in Quiescence

To prune provably hopeless tactical branches, quiescence search incorporates static exchange evaluation and delta boundaries:

1. **SEE Pruning**:
   - When not in check, captures with $\text{SEE}(from, to) < 0$ (e.g. $Q \times P$ on a defended square) are skipped. Because standing pat is available, sacrificing material on a defended square cannot exceed the stand-pat baseline $\alpha$.
2. **Delta Pruning (Future Extension)**:
   - If $S + \Delta_{\text{max}} < \alpha$ where $\Delta_{\text{max}}$ is the maximum possible material gain plus safety margin ($\approx 900 \text{ cp}$ for queen capture or promotion), the entire node may fail low without searching captures.
   - Guarded against endgame queen promotions and positions where checks can prolong the combination.
