# BOSON B3-OTB-2: Q4 Alternative 2 Structural Feasibility Audit Specification
**Execution Protocol & Governance Boundaries**  
**Specification Identifier:** `SPEC-B3-OTB-2-Q4-ALT2-STRUCTURAL-FEASIBILITY-AUDIT`  
**Parent Amendment:** [`docs/B3_OTB_2_Q4_ALTERNATIVE_2_AMENDMENT_SPEC.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_Q4_ALTERNATIVE_2_AMENDMENT_SPEC.md) (`83783a21833489b7634ea3999a70013c5ce20d75`)  
**Parent Specification (Q4 Draft 4):** [`docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md)  
**Ratification Authority:** GPT-1 (Ratified / Ready for Structural Feasibility Audit)  
**Status:** `AUDIT COMPLETE — RESULT: PASS (PRE-LABEL STRUCTURAL ALLOCATION FEASIBILITY VERIFIED)`  

---

> [!IMPORTANT]
> **ABSOLUTE EPISTEMIC & EXECUTION BOUNDARY:**  
> *"Q4 Alternative 2 structural feasibility establishes that the amended integer variable allocation k(g) satisfies all capacity bounds (0 <= k(g) <= F(g)), exactly satisfies cohort volumes (9,965 positions/arm, 1,993 positions/player), minimizes L1 and L2 deviation from baseline K(g) with unique lexicographical tie-breaking, and guarantees zero cross-arm canonical board overlap. Finite-CP feasibility remains unknown until authorized downstream teacher labeling."*

---

## 1. Audit Mandate and Scope

This audit executes the authoritative pre-label structural allocation and feasibility verification for experiment **B3-OTB-2** under the **Alternative 2** ratified amendment protocol.

The scope is strictly restricted to:
1. Verifying deterministic reproduction of frozen source-game selections for Arm A (848 games) and Arm B (848 games).
2. Recovering authoritative frozen baseline integer quotas $K(g)$ from Q2.3 records without reconstruction from averages.
3. Recovering authoritative frozen post-collision structural capacities $F(g)$ from Q4 Draft 4 records.
4. Generating legal candidate occurrences for plies $12 \le \text{ply} \le 120$ in strictly ascending order without within-game canonical deduplication (Q4-I14).
5. Applying frozen external historical (`SYN-20K`, `REAL-20K`, `STRONG-OTB`, `REAL-9965`) and blind benchmark exclusions.
6. Identifying cross-arm canonical collisions ($\mathcal{K}_{\text{collision}}$) and applying frozen `Symmetric-Hash-Parity-v1` ownership (Q4-I15).
7. Computing structural deficits $D(g) = \max(0, K(g) - F(g))$ independently from record data without hardcoding preliminary expectations.
8. Formulating and solving the exact integer optimization problem for $k(g)$ under the complete lexicographical objective hierarchy:
   - Level 1: $\min L_1 = \sum |k(g) - K(g)|$
   - Level 2: $\min L_2 = \sum (k(g) - K(g))^2$ among $L_1$ minimizers
   - Level 3: Lexicographically smallest vector according to coordinate order (Arm A, then Arm B Anand, Carlsen, Caruana, Ding, Nakamura; sorted bytewise by `source_game_key`).
9. Proving uniqueness of the resulting allocation vector $k^*(g)$.
10. Materializing exactly the first $k^*(g)$ occurrences from structural sequence $S_g$ for every game.
11. Verifying complete provenance, ply boundaries ($12 \le \text{ply} \le 120$), and identifier integrity.
12. Verifying zero cross-arm canonical overlap ($\text{Keys}(D_A) \cap \text{Keys}(D_B) \equiv \emptyset$).
13. Conducting a comprehensive distributional audit of position weighting changes and measured associations.
14. Proving bit-for-bit repeatability across two identical, independent audit passes.
15. Verifying zero downstream execution (zero Stockfish invocations, zero teacher labeling, zero training, zero evaluation, zero model modification).

---

## 2. Parent Lineage and Cryptographic Hashes

| Lineage Artifact | File Path | Authoritative SHA-256 Digest | Status |
| :--- | :--- | :--- | :---: |
| **Q4 Draft 4 Spec** | [`docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md) | `e08e711d1a87abe385ec928f90a12777f774329b105556e81a55a424518ee0fc` | `FROZEN PARENT` |
| **Q4 Draft 4 Manifest** | [`data/b3_otb_2/q4_design_amendment_manifest_draft_4.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/q4_design_amendment_manifest_draft_4.json) | `d1a04ad06103f91679ea2cb8d6527729f4f348feae2a8ea5440544e084c29762` | `FROZEN PARENT` |
| **Q4 Alt 2 Spec** | [`docs/B3_OTB_2_Q4_ALTERNATIVE_2_AMENDMENT_SPEC.md`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/docs/B3_OTB_2_Q4_ALTERNATIVE_2_AMENDMENT_SPEC.md) | `335aaed15cb57435050c48f5425f3dc0b7befd38ed8da5ca5afefc30a353b3ee` | `RATIFIED SPEC` |
| **Q4 Alt 2 Manifest** | [`data/b3_otb_2/q4_alternative_2_amendment_manifest.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/q4_alternative_2_amendment_manifest.json) | `01dc6a50719bee72a6ba37e78d09bd03b59a4678f691cf581f9be2e4c129b7a9` | `RATIFIED SPEC` |
| **Q4 Capacity Audit** | [`data/b3_otb_2/q4_candidate_capacity_audit.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/q4_candidate_capacity_audit.json) | `ddbdb0bbe39efe010127066264546674ab372f8e3bb0f406d549f47e646c574c` | `FROZEN RECORD` |
| **Q4 Collision Audit** | [`data/b3_otb_2/q4_collision_ownership_audit.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3_otb_2/q4_collision_ownership_audit.json) | `d9aab87028e2ba43748c67e3acff27aea0ad1e421069576153245bfaba79a499` | `FROZEN RECORD` |
| **Blind Benchmark** | [`data/b3a_blind_test/blind_positions_labeled.json`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/data/b3a_blind_test/blind_positions_labeled.json) | `d8934c5c90d93bf6a8885d56f5d2c24f247466c9b8d33a45ccf5ea3865a5dc82` | `IMMUTABLE` |
| **Production Model** | [`models/boson-v2.nnue`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/models/boson-v2.nnue) | `ef3386104547109445a47257c85afd99beef3cadbf7766566244e76a040dae92` | `IMMUTABLE` |

---

## 3. Mathematical Optimization Formulation & Decomposition Theorem

### 3.1 Global Feasible Domain $\Omega$
The decision vector $k = (k(g))_{g=1}^{1696} \in \mathbb{Z}_{\ge 0}^{1696}$ must satisfy:
1. $0 \le k(g) \le F(g)$ for all $g \in \text{Games}$
2. $\sum_{g \in \text{Arm A}} k(g) = 9,965$
3. $\sum_{g \in \text{Arm B, } p} k(g) = 1,993$ for each $p \in \{\text{Anand, Carlsen, Caruana, Ding, Nakamura}\}$

### 3.2 Additive Group Separability Theorem
Let the 1,696 games be partitioned into 6 disjoint groups:
$G_1 = \text{Arm A}$ (848 games), $G_2 = \text{Anand}$ (170 games), $G_3 = \text{Carlsen}$ (170 games), $G_4 = \text{Caruana}$ (170 games), $G_5 = \text{Ding}$ (169 games), $G_6 = \text{Nakamura}$ (169 games).

**Theorem:** Because:
1. The hard constraints restrict each sub-vector $k|_{G_j}$ independently to $\Omega_j$ with no cross-group coupling constraints ($\Omega = \Omega_1 \times \Omega_2 \times \dots \times \Omega_6$),
2. The Level 1 objective is strictly additive across groups: $L_1(k) = \sum_{j=1}^6 L_{1, j}(k|_{G_j})$,
3. The Level 2 objective is strictly additive across groups: $L_2(k) = \sum_{j=1}^6 L_{2, j}(k|_{G_j})$,
4. The coordinate ordering places all coordinates of $G_1$ before $G_2$, before $G_3$, ..., before $G_6$,

the global lexicographical minimizer on the complete feasible set $\Omega$ is **mathematically identical** to the concatenation of the unique local lexicographical minimizers on each group:
$$k^* = (k_1^*, k_2^*, \dots, k_6^*) \quad \text{where} \quad k_j^* = \text{lexmin} \left( \arg\min_{k_j \in \Omega_{2, j}} L_{2, j}(k_j) \right)$$

### 3.3 Exact Constructive Optimal Form
For each group $G_j$ with target $T_j$:
- For deficit games ($F(g) < K(g)$): Level 1 forces $k(g) = F(g)$.
- For equality games ($F(g) = K(g)$): Capacity forces $k(g) \le K(g)$ while Level 1 forces $k(g) \ge K(g) \implies k(g) = K(g)$.
- For surplus games ($F(g) > K(g)$): The quadratic penalty in Level 2 forces surplus adjustments to be dispersed as $+1$ units. Exactly $\sum_{g \in G_j} D(g)$ surplus games receive $k(g) = K(g) + 1$, and all other surplus games receive $k(g) = K(g)$.
- Level 3 (lexicographically smallest vector): Among all $\binom{|G_{j, \text{surplus}}|}{\sum D(g)}$ binary allocations of $+1$, the unique lexicographical minimizer sets $k(g) = K(g)$ on earlier coordinates and concentrates the $+1$ assignments on the **last** $\sum D(g)$ surplus games.

---

## 4. Dual-Solver Implementation & Uniqueness Proof

To guarantee implementation independence and absolute determinism:
1. **Solver Path 1 (General Dynamic Programming):** Computes suffix minimum costs via backward induction over state offsets and performs lexicographical backtracking to select the minimum $k$ at each coordinate.
2. **Solver Path 2 (Exact Constructive Closed-Form):** Implements the analytical theorem directly.
3. **Equivalence Invariant:** Solver Path 1 and Solver Path 2 must produce bit-for-bit identical vectors across all 1,696 games.

---

## 5. Candidate Materialization & Disjointness Protocol

1. Candidate occurrences are drawn strictly from sequence $S_g$ in frozen ascending halfmove ply order ($12 \le \text{ply} \le 120$).
2. Exactly $k^*(g)$ candidate occurrences are admitted from each game $g$.
3. All identifiers (`source_game_key:decimal(ply)`), canonical board keys, and provenance tags are preserved.
4. Because collision keys are partitioned unambiguously by `Symmetric-Hash-Parity-v1` and non-collision occurrences remain with their origin arm:
   $$\text{Keys}(D_A) \cap \text{Keys}(D_B) \subseteq \text{Keys}(S_A) \cap \text{Keys}(S_B) \equiv \emptyset$$

---

## 6. Zero Downstream Execution Boundary
The audit strictly asserts:
* `stockfish_invocations = 0`
* `teacher_harness_invocations = 0`
* `teacher_label_records = 0`
* `training_invocations = 0`
* `checkpoint_created = false`
* `blind_evaluation_invocations = 0`
* `calibration_invocations = 0`
* `production_model_modified = false`
