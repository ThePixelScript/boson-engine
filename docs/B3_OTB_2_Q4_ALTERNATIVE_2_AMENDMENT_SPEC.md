# BOSON B3-OTB-2: Q4 Alternative 2 Amendment Specification
**Frozen Q4 Collision Ownership + Deterministic Pre-Label Variable Game Allocation**  
**Amendment Identifier:** `B3-OTB-2-Q4-ALT2`  
**Status:** `RATIFIED / READY FOR STRUCTURAL FEASIBILITY AUDIT`  
**Ratification Authority:** GPT-1 (Architectural Ratification & Authorization for Next Stage)  
**Design Phase:** Ratified Amendment Specification Checkpoint (Zero Downstream Execution Authorized)

---

> [!IMPORTANT]
> **GOVERNANCE & RATIFICATION STATUS:**  
> *"GPT-1 has RATIFIED Alternative 2 and authorized the next stage: the Q4 Alternative 2 Pre-Label Structural Allocation and Feasibility Audit. This specification defines the immutable rules, constraints, objectives, and semantic boundaries governing that audit."*  
> **ABSOLUTE EPISTEMIC & EXECUTION BOUNDARY:**  
> *"This task is ONLY to create and archive the ratified amendment specification and machine manifest. DO NOT RUN THE STRUCTURAL ALLOCATION AUDIT IN THIS TASK. No solver/optimizer may be run, no candidate materialized, no Stockfish teacher invoked, no finite-CP dataset constructed, no model trained or evaluated, and no production model altered."*

---

## 1. Scope and Architectural Authority

### 1.1 Scope
This document specifies **Alternative 2** of the **BOSON B3-OTB-2 Q4** design protocol. Alternative 2 resolves the structural feasibility challenge identified under fixed per-game quotas ($K \in \{11, 12\}$) by replacing the fixed per-game quota with a **deterministic pre-label integer allocation** $k(g)$ while strictly freezing the collision ownership mapping, candidate stream semantics, and total cohort volumes established in Q4 Draft 4.

### 1.2 Architectural Authority
- **Authority:** GPT-1 formal architectural review and ratification.
- **Disposition:** `RATIFIED / READY FOR STRUCTURAL FEASIBILITY AUDIT`.
- **Governing Amendment Concept:** Frozen Q4 Collision Ownership (`Symmetric-Hash-Parity-v1`) + Deterministic Pre-Label Variable Game Allocation ($k(g)$).
- **Execution State:** Specification Checkpoint. Implementation and execution remain strictly unauthorized until the dedicated structural feasibility audit task.

---

## 2. Frozen Q4 Inheritance and Upstream Lineage

### 2.1 Upstream Lineage
Q4 Draft 4 serves as the authoritative, ratified parent specification. It remains permanently frozen, immutable, and untouched:

| Artifact Lineage | Path | SHA-256 Digest | Status |
| :--- | :--- | :--- | :---: |
| **Ratified Parent Spec (Q4 Draft 4)** | [`docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md) | `e08e711d1a87abe385ec928f90a12777f774329b105556e81a55a424518ee0fc` | `FROZEN PARENT` |
| **Ratified Parent Manifest (Q4 Draft 4)** | [`data/b3_otb_2/q4_design_amendment_manifest_draft_4.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/q4_design_amendment_manifest_draft_4.json) | `d1a04ad06103f91679ea2cb8d6527729f4f348feae2a8ea5440544e084c29762` | `FROZEN PARENT` |
| **Upstream Q2.3 Spec** | [`docs/B3_OTB_2_SUPERSEDING_DESIGN_SPEC_Q2_3_DRAFT.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_SUPERSEDING_DESIGN_SPEC_Q2_3_DRAFT.md) | `08f7ece7cb47967890690b7d2c007973fe2c2a25df4df6470feec02fc23438af` | `HISTORICAL` |
| **Upstream Q2.3 Manifest** | [`data/b3_otb_2/superseding_design_manifest_q2_3_draft.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/superseding_design_manifest_q2_3_draft.json) | `b3c06d2032b79e5dbf96764699347b104100d5d1c6d4165b2e894e630e270b95` | `HISTORICAL` |
| **Frozen Blind Benchmark** | [`data/b3a_blind_test/blind_positions_labeled.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3a_blind_test/blind_positions_labeled.json) | `d8934c5c90d93bf6a8885d56f5d2c24f247466c9b8d33a45ccf5ea3865a5dc82` | `IMMUTABLE` |
| **Production NNUE Model** | [`models/boson-v2.nnue`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/models/boson-v2.nnue) | `ef3386104547109445a47257c85afd99beef3cadbf7766566244e76a040dae92` | `IMMUTABLE` |

### 2.2 Comprehensive Frozen Rule Inventory
All foundational experimental controls and data pipeline mechanisms from Q4 Draft 4 remain strictly invariant:
1. **Arm A Selected Games:** Exactly the existing 848 classical OTB games (Carlsen-only).
2. **Arm B Selected Games:** Exactly the existing 848 classical OTB games across 5 elite grandmasters.
3. **Frozen Source Populations:** PGN Mentor + TWIC Issues 948–1625 reconciled universe.
4. **Frozen Eligibility Window:** 2013-01-01 through 2025-12-31 inclusive, classical time controls, strict board legality.
5. **Frozen Arm-B Cohort Distribution:** Viswanathan Anand (170 games), Magnus Carlsen (170 games), Fabiano Caruana (170 games), Ding Liren (169 games), Hikaru Nakamura (169 games).
6. **H2H Source-Game Ownership:** Lexicographical `min(canonical_player_id)`.
7. **Candidate Ply Window:** Plies $12 \le \text{ply} \le 120$ inclusive (1-based half-move count).
8. **Candidate Position Identifier:** `candidate_position_id = source_game_key + ":" + decimal(ply)`.
9. **Within-Game Non-Deduplication:** Repeated canonical board keys at different plies remain distinct candidate occurrences (Q4-I14).
10. **Canonical Normalization:** 4-field FEN representation with legal en-passant square normalization (Q4-I11).
11. **Historical Leakage Filtering:** Complete exclusion of `SYN-20K`, `REAL-20K`, `STRONG-OTB`, and `REAL-9965` (Q4-I2).
12. **Blind Leakage Filtering:** Zero GameKey overlap and zero canonical overlap with the Common Blind Benchmark (Q4-I3).
13. **Collision Identification:** Identical cross-arm collision set $\mathcal{K}_{\text{collision}} = \text{Keys}(\mathcal{C}_A^{\text{ext}}) \cap \text{Keys}(\mathcal{C}_B^{\text{ext}})$.
14. **Collision Ownership Mapping:** Frozen `Symmetric-Hash-Parity-v1` evaluated on UTF-8 payload `"B3-OTB-2-Q4-COLLISION-v1:" + canonical_board_key`.
15. **Unambiguous Ownership Invariant:** Exactly one owner per collision key; all candidate occurrences of key $k$ inherit the same owner (Q4-I4, Q4-I15).
16. **Non-Collision Occurrence Ownership:** Non-colliding keys permanently remain with their origin arm.
17. **Pre-Label Canonical Disjointness:** $\text{Keys}(S_A) \cap \text{Keys}(S_B) \equiv \emptyset$ (Q4-I1).
18. **Final Volume Targets:** Exactly 9,965 final positions per arm; exactly 1,993 final positions per Arm-B player (Q4-I10).
19. **Teacher Engine Specification:** Stockfish 19 (`edb0d9d`, SHA-256 `45bc8e49...`, network `nn-1a298aa575a0.nnue`, `go_nodes 100000`, threads 1, hash 64).
20. **NNUE Architecture:** HalfKP ($40960 \times 2 \to 1024 \to 32 \to 32 \to 1$).
21. **Training Contract:** AdamW optimizer, cosine learning rate schedule over 780 steps, batch size 16,384, deterministic seed 42.
22. **Statistical Contract:** Paired difference estimand $\Delta_i = |E_A(i) - T(i)| - |E_B(i) - T(i)|$, 10,000 paired cluster-bootstrap resamples clustered by `source_game_key`.
23. **Teacher Independence:** Structural candidate set, collision ownership, and game allocations are 100% independent of teacher evaluation.
24. **Model Independence:** Data construction is 100% independent of model architecture, weights, and evaluation results.

---

## 3. The Single Changed Rule

Under Q4 Alternative 2, **exactly one** rule of the Q4 specification changes:

$$\text{Fixed per-game quota } K(g) \in \{11, 12\} \quad \longrightarrow \quad \text{Deterministic pre-label variable integer allocation } k(g) \in \mathbb{Z}_{\ge 0}$$

All candidate generation, external filtering, canonical normalization, collision identification, collision ownership partitioning, and cohort volume constraints remain strictly identical to Q4 Draft 4. The allocation $k(g)$ is determined globally and deterministically at the pre-label structural stage prior to any candidate admission or teacher labeling.

---

## 4. Authoritative $K(g)$ Baseline

$K(g)$ is the exact, authoritative integer assigned to game $g$ inherited from the frozen Q2.3/Q3 allocation:
- **Arm A:** Exactly 637 games assigned $K(g) = 12$, and exactly 211 games assigned $K(g) = 11$ ($637 \times 12 + 211 \times 11 = 9,965$).
- **Arm B:** Exactly 170 games per player for Anand, Carlsen, Caruana, and 169 games per player for Ding, Nakamura, with game-level integer quotas summing to exactly 1,993 per player and 9,965 total.

> [!CAUTION]
> **PROHIBITION ON FLOATING-POINT RECONSTRUCTION:**  
> Never reconstruct $K$ as $9965 / 848 \approx 11.751179\dots$. Never infer $K$ using a floating-point average or quotient. $K(g)$ is an authoritative integer record inherited directly from upstream frozen metadata.

---

## 5. Authoritative $F(g)$ Structural Capacity

$F(g)$ is defined as the authoritative frozen Q4 post-collision structural capacity of game $g$:

$$F(g) = \text{structural\_capacity}(g) = |S_g|$$

where $S_g$ is the ordered sequence of candidate **occurrences** from game $g$ across plies $12 \le \text{ply} \le 120$ that survive external historical and blind leakage exclusions and are structurally owned by the arm of game $g$ (either by origin non-collision or by frozen `Symmetric-Hash-Parity-v1` collision ownership).

- Capacities are strictly **occurrence-level** (counting candidate occurrences, not distinct canonical board keys).
- $F(g)$ is frozen and immutable; no collision ownership changes are permitted.

---

## 6. Hard Allocation Variables

For every selected game $g \in \text{Games}_A \cup \text{Games}_B$:
- The decision variable is $k(g) \in \mathbb{Z}_{\ge 0}$ (a non-negative integer representing the exact number of candidate occurrences to be admitted from game $g$).
- Total number of decision variables: $|\text{Games}_A| + |\text{Games}_B| = 848 + 848 = 1,696$ integer variables.

---

## 7. Hard Allocation Constraints

The complete feasible allocation set $\Omega$ is defined as all integer vectors $k = (k(g))_{g \in \text{Games}}$ that satisfy all of the following hard constraints simultaneously:

1. **Individual Structural Capacity Upper Bound:**
   $$\forall g \in \text{Games}_A \cup \text{Games}_B: \quad 0 \le k(g) \le F(g)$$
2. **Arm A Total Volume Constraint:**
   $$\sum_{g \in \text{Games}_A} k(g) = 9,965$$
3. **Arm B Total Volume Constraint:**
   $$\sum_{g \in \text{Games}_B} k(g) = 9,965$$
4. **Arm B Player Quota Constraints:**
   For each player $p \in \{\text{ANAND}, \text{CARLSEN}, \text{CARUANA}, \text{DING}, \text{NAKAMURA}\}$:
   $$\sum_{g \in \text{Games}_{B, p}} k(g) = 1,993$$

Any allocation vector failing any of these constraints is strictly infeasible ($k \notin \Omega$).

---

## 8. Exact Three-Level Objective Hierarchy

The optimal allocation vector $k^*$ is defined by a strict lexicographic hierarchy of three optimization levels applied over the complete feasible set $\Omega$:

### Level 1: Minimization of Absolute Deviation ($L_1$)
Minimize the total absolute deviation from the authoritative baseline $K(g)$:
$$\min_{k \in \Omega} L_1(k) = \sum_{g} |k(g) - K(g)|$$
Let $\Omega_1 = \arg\min_{k \in \Omega} L_1(k)$ be the set of all $L_1$ minimizers.

### Level 2: Minimization of Squared Deviation ($L_2$)
Among all $L_1$ minimizers, minimize the total squared deviation from baseline $K(g)$:
$$\min_{k \in \Omega_1} L_2(k) = \sum_{g} (k(g) - K(g))^2$$
Let $\Omega_2 = \arg\min_{k \in \Omega_1} L_2(k)$ be the set of all joint $L_1 / L_2$ minimizers.

### Level 3: Lexicographically Smallest Allocation Vector
Among all joint $L_1 / L_2$ minimizers, select the unique vector that is lexicographically smallest according to the authoritative coordinate ordering:
$$k^* = \text{lexmin}_{k \in \Omega_2} (k)$$

> [!IMPORTANT]
> **NO HEURISTIC TIE-BREAKING:**  
> All three levels apply globally across the complete feasible set. No solver-dependent, stochastic, or unspecified tie-breaking is permitted. The optimal allocation $k^*$ is mathematically unique.

---

## 9. Exact Coordinate Ordering for Lexicographical Tie-Breaking

The coordinate ordering of games for Level 3 lexicographical evaluation is strictly specified as follows:

1. **Arm A Games:**
   All 848 Arm A games, sorted by `source_game_key` in bytewise ascending order (standard UTF-8 byte comparison).
2. **Arm B Games:**
   Grouped by player cohort in exact alphabetical order:
   - Cohort 1: `ANAND_VISWANATHAN`
   - Cohort 2: `CARLSEN_MAGNUS`
   - Cohort 3: `CARUANA_FABIANO`
   - Cohort 4: `DING_LIREN`
   - Cohort 5: `NAKAMURA_HIKARU`
   Within each player cohort, games are sorted by `source_game_key` in bytewise ascending order.

---

## 10. Deficit Games Handling ($F(g) < K(g)$)

For games where post-collision structural capacity is strictly less than the original target:
$$F(g) < K(g)$$
- The hard constraint forces $k(g) \le F(g) < K(g)$.
- Game $g$ contributes strictly fewer candidate occurrences than its baseline target $K(g)$.
- The individual game deficit is $D(g) = K(g) - F(g) > 0$.
- Under Level 1 minimization, any feasible allocation minimizing $L_1$ must allocate $k(g) = F(g)$ (allocating $k(g) < F(g)$ would unnecessarily increase downward deviation and strictly worsen $L_1$).
- Deficit positions cannot be backfilled by cross-game borrowing, cross-player borrowing, or game replacement.

---

## 11. Surplus and Baseline Games Handling ($F(g) \ge K(g)$)

For games where post-collision structural capacity meets or exceeds baseline:
$$F(g) \ge K(g)$$
- $K(g)$ serves as the ideal baseline.
- To satisfy the exact arm target ($\sum k = 9,965$) and player targets ($\sum k = 1,993$), surplus capacity from games with $F(g) > K(g)$ within the same constrained group is allocated to compensate for the deficits of $F < K$ games.
- Level 2 ($L_2$) minimization penalizes large deviations quadratically, forcing surplus allocations to be dispersed evenly across available games (favoring multiple $+1$ adjustments rather than concentrated $+2$ or $+3$ adjustments).
- Level 3 lexicographical ordering deterministically resolves which games receive surplus allocations.
- Prohibitions: No post-label refill. No candidate movement between games. No game replacement. No teacher-guided rescue.

---

## 12. Formal Occurrence Semantics and Identity Hierarchy

All capacity accounting, candidate admission, and collision partitioning are strictly governed by candidate occurrence semantics:

1. **Candidate Occurrence Identity:**
   Uniquely identified by `candidate_position_id`:
   $$\text{candidate\_position\_id} = \text{source\_game\_key} + \text{":"} + \text{decimal(ply)}$$
   with $12 \le \text{ply} \le 120$.
2. **Within-Game Repeated Canonical Boards (Q4-I14):**
   Repeated `canonical_board_key` occurrences at different plies within the same game are **NOT** deduplicated. They remain distinct candidate occurrences.
3. **Collision Occurrence Inheritance (Q4-I15):**
   If canonical collision key $c \in \mathcal{K}_{\text{collision}}$ occurs $m(g, c)$ times in game $g$, all $m$ occurrences inherit the exact same frozen owner $\text{Owner}(c)$ computed by `Symmetric-Hash-Parity-v1`.
4. **Three Distinct Identifier Domains:**
   $$\text{source\_game\_key} \neq \text{candidate\_position\_id} \neq \text{canonical\_board\_key}$$

---

## 13. Candidate Materialization Rules

In the future audit and downstream execution, after the optimal integer vector $k^*(g)$ is determined:
1. For each game $g$, take the pre-label structural candidate sequence $S_g$ containing only valid, structurally owned candidate occurrences.
2. Ensure candidate occurrences in $S_g$ are sorted in strictly ascending halfmove ply order ($12 \le \text{ply} \le 120$).
3. Materialize exactly the **first $k^*(g)$** candidate occurrences from $S_g$.
4. Retain all immutable identifiers: `candidate_position_id`, `source_game_key`, `canonical_board_key`, and collision ownership tag.
5. No candidate occurrence may be synthesized, interpolated, or drawn from outside the frozen candidate stream.

---

## 14. Complete Provenance Preservation

Every admitted candidate occurrence must retain full historical and cryptographic provenance:
- Source game identifier (`source_game_key`)
- Candidate position identifier (`candidate_position_id`)
- Halfmove ply index ($12 \le \text{ply} \le 120$)
- Canonical board representation (4-field normalized FEN)
- ECO code, white player, black player, event, and match date
- Collision status (`non_collision` vs. `collision`)
- Collision ownership resolution (`Symmetric-Hash-Parity-v1` hash value and designated owner)

---

## 15. Frozen Collision Ownership Preservation

The collision handling protocol remains 100% frozen as specified in Q4 Draft 4:
- Collision set: $\mathcal{K}_{\text{collision}} = \text{Keys}(\mathcal{C}_A^{\text{ext}}) \cap \text{Keys}(\mathcal{C}_B^{\text{ext}})$.
- Ownership partition: `Symmetric-Hash-Parity-v1` on UTF-8 `"B3-OTB-2-Q4-COLLISION-v1:" + canonical_board_key`.
- Invariant Q4-I4: Exactly one owner per collision key.
- Invariant Q4-I13: Collision ownership is a function only of the canonical key and frozen namespace.
- Invariant Q4-I15: All occurrences across all games inherit the same owner.
- Non-collision occurrences permanently remain with their origin arm.
- No collision ownership changes are made or permitted.

---

## 16. Guaranteed Pre-Label Canonical Disjointness

Because every cross-arm collision key is assigned to exactly one owner, and non-collision positions belong only to their origin arm:
$$\text{Keys}(S_A) \cap \text{Keys}(S_B) \equiv \emptyset$$
Since the admitted candidate set for any game $g$ is a prefix subset of $S_g$ (admitting the first $k(g) \le F(g) = |S_g|$ occurrences), canonical disjointness is guaranteed across all admitted sets:
$$\text{Keys}(D_A) \cap \text{Keys}(D_B) \subseteq \text{Keys}(S_A) \cap \text{Keys}(S_B) \equiv \emptyset$$
Final canonical disjointness holds mathematically prior to teacher labeling.

---

## 17. Mathematical Feasibility Existence Theorem

### Theorem (Integer Bounded Exact-Sum Allocation Feasibility)
Let $G$ be a finite set of games. For each game $g \in G$, let $F(g) \in \mathbb{Z}_{\ge 0}$ be the non-negative integer capacity upper bound, and let $T \in \mathbb{Z}_{\ge 0}$ be the exact target sum. There exists an integer allocation vector $k = (k(g))_{g \in G}$ satisfying:
$$0 \le k(g) \le F(g) \quad \forall g \in G \quad \text{and} \quad \sum_{g \in G} k(g) = T$$
if and only if:
$$0 \le T \le \sum_{g \in G} F(g)$$

### Proof
- **Necessity:** If such an allocation $k$ exists, then $T = \sum_{g \in G} k(g) \le \sum_{g \in G} F(g)$, and $k(g) \ge 0 \implies T \ge 0$.
- **Sufficiency:** Construct an allocation greedily: order games arbitrarily $g_1, g_2, \dots, g_m$. Set $k(g_i) = \min(F(g_i), T - \sum_{j=1}^{i-1} k(g_j))$. Since $0 \le T \le \sum_{g \in G} F(g)$ and all $F(g)$ are non-negative integers, each $k(g_i)$ is an integer satisfying $0 \le k(g_i) \le F(g_i)$ and $\sum_{i=1}^m k(g_i) = T$. $\blacksquare$

### Application to B3-OTB-2 Alternative 2
Under authoritative Q4 structural capacities:
1. **Arm A:**
   Target $T_A = 9,965$. Authoritative total capacity:
   $$\sum_{g \in \text{Games}_A} F(g) = 56,857 \ge 9,965$$
2. **Arm B Player Cohorts (Target $T_p = 1,993$ each):**
   - **Viswanathan Anand:** $\sum F_g = 10,524 \ge 1,993$
   - **Magnus Carlsen:** $\sum F_g = 6,336 \ge 1,993$
   - **Fabiano Caruana:** $\sum F_g = 12,951 \ge 1,993$
   - **Ding Liren:** $\sum F_g = 12,223 \ge 1,993$
   - **Hikaru Nakamura:** $\sum F_g = 12,404 \ge 1,993$
   Total Arm B capacity: $\sum_{g \in \text{Games}_B} F(g) = 54,438 \ge 9,965$.

Therefore, the complete hard allocation constraint system possesses at least one valid integer solution $\Omega \neq \emptyset$, assuming authoritative $F(g)$ records from Q4 Draft 4 are correct.

> [!NOTE]
> **EPISTEMIC BOUNDARY:**  
> This is a mathematical existence theorem, NOT an executed allocation. No allocation vector $k(g)$ is generated or selected in this task.

---

## 18. Deficit Metric Formal Definition

For each game $g$, the individual structural deficit $D(g)$ is formally defined as:

$$D(g) = \max(0, K(g) - F(g))$$

The future structural allocation audit MUST derive the following quantities from exact record data:
- Arm A total deficit: $\sum_{g \in \text{Games}_A} D(g)$
- Arm B total deficit: $\sum_{g \in \text{Games}_B} D(g)$
- Global total deficit: $\sum_{g \in \text{Games}} D(g)$

### Preliminary Derived Expectations (Informational Only)
GPT-1 provided the following derived expectations from Q4 evidence:
- Arm A expected deficit: 13
- Arm B expected deficit: 21
- Total expected deficit: 34
- Expected minimum $L_1$: 68

> [!CAUTION]
> **PROHIBITION ON HARDCODING:**  
> These expectations must NOT be embedded as hardcoded optimization constants or constraints. The future structural audit must independently derive them from the exact authoritative $K(g)$ and $F(g)$ records.

---

## 19. The $L_1$ Mathematical Identity

Within each independently constrained group $G$ (Arm A, or an individual Arm-B player cohort), the sum of allocations equals the sum of baselines:
$$\sum_{g \in G} k(g) = \sum_{g \in G} K(g) = T_G$$
Rearranging terms across positive deviations ($k(g) > K(g)$) and negative deviations ($k(g) < K(g)$):
$$\sum_{g \in G: k(g) > K(g)} (k(g) - K(g)) = \sum_{g \in G: k(g) < K(g)} (K(g) - k(g))$$
That is:
$$\text{Total Upward Deviation} \equiv \text{Total Downward Deviation}$$
Therefore, the total absolute deviation $L_1$ satisfies the exact mathematical identity:
$$L_1 = \sum_{g \in G} |k(g) - K(g)| = 2 \times \sum_{g \in G: k(g) < K(g)} (K(g) - k(g)) = 2 \times \text{Total Downward Deviation}$$

When downward deviation is strictly restricted to deficit games ($k(g) = F(g)$ for $F(g) < K(g)$, and $k(g) \ge K(g)$ for $F(g) \ge K(g)$), the theoretical minimum $L_1$ is exactly:
$$L_1^* = 2 \times \sum_{g \in G} D(g)$$

---

## 20. Scientific Interpretation and Non-Neutrality Mandate

1. **Invariance of Source Populations:**
   Source game selections remain identical (848 games in Arm A, 848 games in Arm B).
2. **Invariance of Primary Scientific Hypothesis:**
   The fundamental experimental comparison remains:
   $$\text{Carlsen-only classical OTB composition} \quad \text{vs.} \quad \text{Diversified 5-elite classical OTB composition}$$
3. **Change in Within-Game Weighting:**
   Because fixed $K(g) \in \{11, 12\}$ is replaced by variable $k(g)$, the exact within-game position weighting changes.

> [!WARNING]
> **NON-NEUTRALITY MANDATE:**  
> It is strictly forbidden to claim or document that the training distribution is "unchanged", "neutral", "imperceptibly altered", or "statistically unaffected" prior to empirical measurement. The distributional impact must be rigorously audited and measured in the subsequent audit task.

---

## 21. Mandatory Future Distributional Audit Measurements

The future structural allocation audit must measure and report the following distributions:

1. **Per-Game Records:**
   For every selected game $g$: authoritative $K(g)$, authoritative $F(g)$, optimal allocation $k^*(g)$, and deviation $\delta(g) = k^*(g) - K(g)$.
2. **Global Summary Statistics:**
   - $\min \delta(g)$, $\max \delta(g)$
   - Count of games with $\delta(g) > 0$ (increased games)
   - Count of games with $\delta(g) < 0$ (decreased games)
   - Count of games with $\delta(g) = 0$ (unchanged games)
   - Total baseline $\sum K(g)$, total allocation $\sum k^*(g)$
   - Realized $L_1 = \sum |\delta(g)|$
   - Realized $L_2 = \sum (\delta(g))^2$
3. **Arm-Level Distributions (Arm A and Arm B):**
   - Empirical distribution of $K(g)$, $k^*(g)$, and $\delta(g)$
   - Mean, median, standard deviation, min, max
   - Position concentration metrics (Gini coefficient, Herfindahl-Hirschman Index, entropy)
4. **Player-Level Distributions (Arm B Cohorts):**
   - Anand, Carlsen, Caruana, Ding, Nakamura reported separately with identical distribution and concentration metrics.
5. **Association Analyses:**
   Empirically measure statistical associations of $k^*(g)$ and $\delta(g)$ with:
   - Structural capacity $F(g)$
   - Game length (total halfmove ply count)
   - Collision incidence (number of colliding positions in game)
   - Player cohort
   - Game year / era
   - Tournament / event
   *Prohibition: Do not infer or assert any correlation or lack of correlation in advance.*

---

## 22. Determinism and Implementation Independence

- The optimization specification $(\Omega, L_1, L_2, \text{lexmin})$ defines a strictly unique mathematical solution $k^*$.
- Any standard optimization method (integer linear programming, dynamic programming, or lexicographical branch-and-bound) conforming to this specification will produce the identical integer vector.
- Independent implementations across different platforms, languages, and solver libraries must yield bit-for-bit identical allocation results.

---

## 23. Exact Bit-for-Bit Repeatability

- Zero random seeds are used during optimization.
- Zero floating-point operations participate in feasibility checks or tie-breaking.
- Tie-breaking is governed strictly by integer coordinate comparisons across sorted UTF-8 byte keys.

---

## 24. Comprehensive Fail-Closed Governance

The future structural allocation audit must immediately terminate with **`STATUS: FAIL-CLOSED`** if any of the following conditions occur:
1. Exact baseline $K(g)$ cannot be recovered or verified against frozen Q2.3/Q3 records.
2. Authoritative $F(g)$ capacity records are missing, incomplete, corrupt, or contradictory.
3. No feasible integer allocation vector exists ($\Omega = \emptyset$).
4. Any hard constraint is violated ($k(g) < 0$, $k(g) > F(g)$, arm totals $\ne 9,965$, player totals $\ne 1,993$).
5. Optimization procedure produces non-deterministic or solver-dependent results.
6. An unresolved tie remains after Level 3 lexicographical ordering.
7. Candidate occurrence provenance fails for any position.
8. Any admitted candidate occurrence has halfmove ply outside $12 \le \text{ply} \le 120$.
9. Candidate position identifier format `<source_game_key>:<ply>` fails.
10. Intra-game repeated canonical keys are deduplicated.
11. Frozen `Symmetric-Hash-Parity-v1` collision ownership map is altered or misapplied.
12. Multi-occurrence collision inheritance fails (Q4-I15 violation).
13. Any canonical board overlap exists between Arm A and Arm B ($\text{Keys}(S_A) \cap \text{Keys}(S_B) \neq \emptyset$).
14. Teacher evaluations or NNUE model weights participate in variable allocation.
15. Frozen parent specification (Q4 Draft 4) is mutated or exhibits SHA-256 mismatch.
16. Any downstream execution (teacher labeling, training, evaluation) is initiated.

---

## 25. Downstream Execution Boundary and Authorization Scope

### 25.1 Present Checkpoint Boundary
This checkpoint is restricted **EXCLUSIVELY** to creating and archiving:
- `docs/B3_OTB_2_Q4_ALTERNATIVE_2_AMENDMENT_SPEC.md`
- `data/b3_otb_2/q4_alternative_2_amendment_manifest.json`

### 25.2 Explicit Prohibitions
During this task, the following actions are strictly prohibited:
- Running an optimizer or integer solver
- Generating or selecting any $k$-vector
- Materializing candidate occurrences
- Extracting or modifying source games
- Invoking Stockfish or teacher harness
- Creating teacher label records
- Constructing finite-CP training datasets
- Initializing or training NNUE models
- Evaluating blind test benchmarks
- Calibrating models
- Modifying production model `models/boson-v2.nnue`
- Modifying Q4 Draft 4 artifacts

### 25.3 Next Authorized Stage
The next authorized stage is the **Q4 Alternative 2 Pre-Label Structural Allocation and Feasibility Audit**, to be conducted in a separate, dedicated execution task under the governance of this specification.

---

## 26. Formal Architectural Diff: Q4 Draft 4 vs. Q4 Alternative 2

| Protocol Dimension | Q4 Draft 4 (Frozen Parent) | Q4 Alternative 2 (Amended) | Architectural Rationale |
| :--- | :--- | :--- | :--- |
| **Per-Game Target Rule** | Fixed per-game integer $K(g) \in \{11, 12\}$ | Deterministic pre-label variable integer $k(g) \in \mathbb{Z}_{\ge 0}$ | Resolves capacity bottlenecks in games with $F(g) < K(g)$ without replacing games |
| **Feasibility Criterion** | $F(g) \ge K(g)$ for all 848 games per arm | $0 \le k(g) \le F(g)$ with exact cohort sums $\sum k = 9,965$ | Permits integer trade-offs within the same player/arm cohort |
| **Objective Hierarchy** | None (uniform per-game quota) | Lexicographic 3-level optimization: $\min L_1 \to \min L_2 \to \text{lexmin}$ coordinate order | Eliminates solver ambiguity; minimizes deviation from original experimental design |
| **Surplus Allocation** | None allowed (exact $K$ per game) | Distributed deterministically to games with $F(g) \ge K(g)$ | Maintains exact 9,965 arm volume and 1,993 player volume |
| **Candidate Materialization**| First $K(g)$ finite-CP scan from $S_g$ | First $k^*(g)$ occurrences from structural sequence $S_g$ | Exact pre-label candidate admission tied to optimal allocation $k^*(g)$ |
| **Collision Ownership** | `Symmetric-Hash-Parity-v1` | `Symmetric-Hash-Parity-v1` (**IDENTICAL / FROZEN**) | Zero change to collision mechanism |
| **Candidate Occurrences** | Plies 12..120, no intra-game deduplication | Plies 12..120, no intra-game deduplication (**IDENTICAL / FROZEN**) | Preserves occurrence semantics |
| **Cohort Volumes** | 9,965/arm, 1,993/player | 9,965/arm, 1,993/player (**IDENTICAL / FROZEN**) | Preserves global experimental scale |
| **Disjointness Guarantee** | Pre-label structural canonical disjointness | Pre-label structural canonical disjointness (**IDENTICAL / FROZEN**) | Guaranteed by frozen collision ownership |
| **Teacher Independence** | Pre-label collision ownership independent of teacher | Allocation $k^*(g)$ and ownership independent of teacher (**PRESERVED**) | Teacher never participates in allocation |
