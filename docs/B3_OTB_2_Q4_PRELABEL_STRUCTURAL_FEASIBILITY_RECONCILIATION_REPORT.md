# BOSON B3-OTB-2: Q4 Pre-Label Structural Feasibility Reconciliation Report
**Evidence Reconciliation, Per-Game Derivations, and Authoritative Invariant Verdict**  
**Audit Identifier:** `AUDIT-B3-OTB-2-Q4-PRELABEL-STRUCTURAL-FEASIBILITY-RECONCILIATION`  
**Execution Timestamp:** 2026-09-30T12:44:39Z  
**Governing Design Specification:** `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` (Ratified by GPT-1)  
**Reconciliation Specification:** `docs/B3_OTB_2_Q4_PRELABEL_STRUCTURAL_FEASIBILITY_RECONCILIATION_SPEC.md`  
**Parent Checkpoint:** `aa07ae8c89a01fa6e84f92e01eacb275d4c42c1e`  

---

## 1. Executive Summary & Reconciliation Verdict

$$\mathbf{RECONCILIATION\;DISPOSITION:\;FAIL-CLOSED\;(12\;SHORTFALLS)}$$
$$\mathbf{AUDIT\;STATUS:\;EVIDENCE\;RECONCILED\;\&\;VERIFIED}$$

This report resolves the conditional review by GPT-1 on committed Q4 pre-label structural audit checkpoint `aa07ae8c89a01fa6e84f92e01eacb275d4c42c1e`. Strictly adhering to the offline evidence reconciliation mandate, no new experiments were run, no source populations or cohort quotas were altered, no design parameters were relaxed, and zero teacher/Stockfish/training execution was performed.

### Key Reconciliation Findings:
1. **Issue 1 Resolution (Arm-B Failure Count = Exactly 6 Games):**
   * The underlying 848 per-game records in Arm B prove:
     $$\text{selected} = 848,\quad \text{feasible} (c \ge K) = 842,\quad \text{infeasible} (c < K) = 6$$
     $$\text{selected} = \text{feasible} + \text{infeasible} \quad (848 = 842 + 6)$$
   * Individual evaluation of the 7 records cited in GPT-1's conditional review confirms that **Anand rank 24 is completely FEASIBLE** ($c = 49 \ge K = 12$, surplus +37). Its inclusion in GPT-1's query list was caused by conflation with Q3's historical note of "24 rescued Anand H2H games". Zero Anand games fail under Q4.
   * The true count of Arm-B infeasible games is **exactly 6** (4 Carlsen, 1 Caruana, 1 Ding).
   * Arm-A underlying records likewise prove: $\text{selected} = 848$, $\text{feasible} = 842$, $\text{infeasible} = 6$ ($848 = 842 + 6$).
   * Total shortfalls across both arms = **12 games**.

2. **Issue 2 Resolution (Q3 vs. Q4 Candidate Occurrence Identity = 100% Bit-for-Bit Identical):**
   * Candidate occurrence identity sets ($12 \le \text{ply} \le 120$, `source_game_key:ply`) were independently derived from the authoritative underlying records and selections:
     * **Arm A:** $\text{Q3\_RAW} = 65,464$, $\text{Q4\_RAW} = 65,464$ (discrepancy = 0).
       $\text{Historical Exclusions} = 809$, $\text{Blind Exclusions} = 6$, $\text{Intersection} = 0$, $\text{Unique Exclusions} = 815$.
       $\text{Q3\_EXT} = 64,649$, $\text{Q4\_EXT} = 64,649$ (discrepancy = 0).
       $$\text{Q3\_RAW} == \text{Q4\_RAW},\quad \text{Q3\_EXT} == \text{Q4\_EXT}$$
     * **Arm B:** $\text{Q3\_RAW} = 63,272$, $\text{Q4\_RAW} = 63,272$ (discrepancy = 0).
       $\text{Historical Exclusions} = 906$, $\text{Blind Exclusions} = 3$, $\text{Intersection} = 0$, $\text{Unique Exclusions} = 909$.
       $\text{Q3\_EXT} = 62,363$, $\text{Q4\_EXT} = 62,363$ (discrepancy = 0).
       $$\text{Q3\_RAW} == \text{Q4\_RAW},\quad \text{Q3\_EXT} == \text{Q4\_EXT}$$
   * There are zero missing occurrences, zero extra occurrences, and zero classification discrepancies.

3. **Collision Ownership & Final Canonical Disjointness:**
   * Post-exclusion distinct canonical keys: Arm A = 62,456; Arm B = 59,858.
   * True cross-arm collision keys: $|\mathcal{K}_{\text{collision}}| = 14,591$.
   * `Symmetric-Hash-Parity-v1` applied strictly to $\mathcal{K}_{\text{collision}}$: Arm A owned = 7,304 (50.06%), Arm B owned = 7,287 (49.94%).
   * Non-collision keys retain originating arm ownership without partition.
   * Final post-ownership canonical keys: Arm A = 55,169; Arm B = 52,554.
   * **Final Canonical Disjointness:** $\mathcal{K}_{A,\text{final}} \cap \mathcal{K}_{B,\text{final}} = \mathbf{0}$ (Strictly Disjoint).

4. **Zero Downstream Execution:**
   * Zero teacher invocations, zero Stockfish runs, zero training runs, zero blind evaluations.

---

## 2. Reconciliation Issue 1: Arm-B Failure Count Derivation

### 2.1 Individual Evaluation of the Seven Named Records
Below is the record-level evaluation of each record cited in GPT-1's review:

| # | Query Record | Stable Source Game Key | Player | Cohort Rank | Arm-B Overall Rank | Assigned $K$ | Pre-Collision Admissible | Collision Owned / Lost | Final Structural Capacity $c$ | Shortfall ($c - K$) | Predicate & Evaluation |
| :-: | :--- | :--- | :--- | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :--- |
| **1** | **Anand rank 24** | `0798d9f797f8feb2...` | ANAND | 24 | 24 | 12 | 52 | 5 / 3 | **49** | **+37** | **$49 \ge 12 \implies$ FEASIBLE (NOT A SHORTFALL)** |
| **2** | **Carlsen rank 173** | `00bab15ed5b14c9a...` | CARLSEN | 3 | 173 | 12 | 16 | 6 / 10 | **6** | **-6** | $6 < 12 \implies$ **SHORTFALL (Infeasible)** |
| **3** | **Carlsen rank 183** | `046af20d5a74cdeb...` | CARLSEN | 13 | 183 | 12 | 35 | 11 / 24 | **11** | **-1** | $11 < 12 \implies$ **SHORTFALL (Infeasible)** |
| **4** | **Carlsen rank 197** | `081b6da9a5ade734...` | CARLSEN | 27 | 197 | 12 | 27 | 11 / 16 | **11** | **-1** | $11 < 12 \implies$ **SHORTFALL (Infeasible)** |
| **5** | **Carlsen rank 238** | `168bc79f06f6ff31...` | CARLSEN | 68 | 238 | 12 | 27 | 11 / 16 | **11** | **-1** | $11 < 12 \implies$ **SHORTFALL (Infeasible)** |
| **6** | **Caruana rank 119** | `1bfb936ea9f6f0ae...` | CARUANA | 119 | 459 | 12 | 8 | 0 / 2 | **6** | **-6** | $6 < 12 \implies$ **SHORTFALL (Infeasible)** |
| **7** | **Ding rank 166** | `307587b4879d1d20...` | DING | 166 | 676 | 11 | 6 | 0 / 1 | **5** | **-6** | $5 < 11 \implies$ **SHORTFALL (Infeasible)** |

> [!NOTE]
> **Root Cause Explanation for Anand Rank 24:**
> Anand rank 24 has $c = 49$ structural candidates against assigned $K = 12$, providing a surplus of $+37$ candidates. In the Q4 audit report, Table 4 documented that in Q3 Anand suffered 24 H2H depletion failures, and that under Q4 parity all 24 were rescued (0 failures remaining). GPT-1's review misread "Anand H2H Failures: 24 games" as an active failure "Anand rank 24: c<K". In reality, **zero Anand games fail in Arm B**.

### 2.2 Complete Enumeration of Arm-B Infeasible Games (Exactly 6 games)
| Overall Rank | Player | Player Rank | Source Game Key | Assigned $K$ | Final Structural Capacity $c$ | Shortfall | White | Black | Total Plies |
| :---: | :--- | :---: | :--- | :---: | :---: | :---: | :--- | :--- | :---: |
| 173 | CARLSEN_MAGNUS | 3 | `00bab15ed5b14c9a...` | 12 | 6 | -6 | Ding Liren | Carlsen,M | 27 |
| 183 | CARLSEN_MAGNUS | 13 | `046af20d5a74cdeb...` | 12 | 11 | -1 | Giri,A | Carlsen,M | 46 |
| 197 | CARLSEN_MAGNUS | 27 | `081b6da9a5ade734...` | 12 | 11 | -1 | Topalov,V | Carlsen,M | 38 |
| 238 | CARLSEN_MAGNUS | 68 | `168bc79f06f6ff31...` | 12 | 11 | -1 | Karjakin,Sergey | Carlsen,M | 38 |
| 459 | CARUANA_FABIANO | 119 | `1bfb936ea9f6f0ae...` | 12 | 6 | -6 | Ding Liren | Caruana,F | 19 |
| 676 | DING_LIREN | 166 | `307587b4879d1d20...` | 11 | 5 | -6 | Ding Liren | So,W | 17 |

### 2.3 Complete Enumeration of Arm-A Infeasible Games (Exactly 6 games)
| Rank | Source Game Key | Assigned $K$ | Final Structural Capacity $c$ | Shortfall | White | Black | Total Plies |
| :---: | :--- | :---: | :---: | :---: | :--- | :--- | :---: |
| 3 | `00bab15ed5b14c9a...` | 12 | 9 | -3 | Ding Liren | Carlsen,M | 27 |
| 20 | `0509f66abf9b5e2b...` | 12 | 6 | -6 | Carlsen,M | Nakamura,Hi | 41 |
| 66 | `13f2c8c0e26ff64c...` | 12 | 11 | -1 | Rapport,R | Carlsen,M | 36 |
| 140 | `2ac07be82418b825...` | 12 | 11 | -1 | Carlsen,M | Dubov,Daniil | 38 |
| 183 | `3760bb39b56d4e1c...` | 12 | 6 | -6 | Carlsen,M | Anand,V | 32 |
| 666 | `c931ba0c235ac128...` | 11 | 8 | -3 | Radjabov,T | Carlsen,M | 38 |

---

## 3. Reconciliation Issue 2: Q3 vs. Q4 Candidate Accounting

### 3.1 Occurrence-Level Comparison
Candidate positions are strictly identified by `candidate_position_id = <source_game_key>:<decimal_ply>` for $12 \le \text{ply} \le 120$.

| Stream Identity | Arm A (Carlsen-848) | Arm B (Multi-Elite-848) | Exact Equivalence Verdict |
| :--- | :---: | :---: | :---: |
| **Q3 Raw Candidates** | 65,464 | 63,272 | Reference Baseline |
| **Q4 Raw Candidates** | 65,464 | 63,272 | $\text{Q3\_RAW} == \text{Q4\_RAW}$ (0 discrepancies) |
| **Historical Exclusions ($H$)** | 809 | 906 | Frozen Historical Datasets |
| **Blind Exclusions ($B$)** | 6 | 3 | Frozen Blind Benchmark |
| **Exclusion Intersection ($H \cap B$)** | 0 | 0 | Zero overlapping exclusions |
| **Unique Exclusions ($|H \cup B|$)** | 815 | 909 | $|H| + |B| - |H \cap B|$ |
| **Q3 External Admissible** | 64,649 | 62,363 | Reference Baseline |
| **Q4 External Admissible** | 64,649 | 62,363 | $\text{Q3\_EXT} == \text{Q4\_EXT}$ (0 discrepancies) |

### 3.2 Record-Level Discrepancy Inventory
* `missing_from_Q4_raw`: **0**
* `extra_in_Q4_raw`: **0**
* `missing_from_Q4_external`: **0**
* `extra_in_Q4_external`: **0**
* `historical_exclusion_discrepancy`: **0**
* `blind_exclusion_discrepancy`: **0**
* `both_historical_and_blind`: **0**
* `other_frozen_rule_discrepancy`: **0**

Under frozen Q4 Draft 4 rules, raw candidate generation and external exclusion filtering are identical to Q3. The actual occurrence identity sets for both arms are **100% bit-for-bit identical**. The alternative figures mentioned in GPT-1's review prompt (e.g. 65,301 raw / 227 blind / 425 historical) do not correspond to any committed artifact or underlying data record and have zero empirical basis.

---

## 4. Collision Verification & Disjointness Invariant Summary

1. **Collision Set Identification:**
   * Arm A post-exclusion distinct canonical keys: **62,456**
   * Arm B post-exclusion distinct canonical keys: **59,858**
   * True cross-arm collision keys: $|\mathcal{K}_{\text{collision}}| = \mathcal{K}_A \cap \mathcal{K}_B = \mathbf{14,591}$
2. **Symmetric-Hash-Parity-v1 Assignment:**
   * Namespace: `B3-OTB-2-Q4-COLLISION-v1:`
   * Big-endian uint32 parity: bit 0 $= 0 \implies \text{ARM\_A}$, bit 0 $= 1 \implies \text{ARM\_B}$
   * Arm-A Owned Collision Keys: **7,304** (50.06%)
   * Arm-B Owned Collision Keys: **7,287** (49.94%)
   * Exactly one owner per key: **Verified True**
   * Non-collision partitioning applied: **False (0 keys reassigned)**
3. **Occurrence-Level Consistency:**
   * All occurrences of each collision key inherit identical ownership.
   * Repeated within-game canonical boards are preserved as distinct occurrences ($1,061$ in Arm A, $1,137$ in Arm B).
   * Occurrences in different games inherit the identical owner.
   * Zero post-ownership transfers.
4. **Final Canonical Disjointness:**
   * Arm A Final Keys: $47,865 \text{ (non-collision)} + 7,304 \text{ (owned collision)} = \mathbf{55,169}$
   * Arm B Final Keys: $45,267 \text{ (non-collision)} + 7,287 \text{ (owned collision)} = \mathbf{52,554}$
   * Cross-Arm Overlap: $\mathcal{K}_{A,\text{final}} \cap \mathcal{K}_{B,\text{final}} = \mathbf{0}$ (Strictly Disjoint Verified).

---

## 5. Verification of Required Invariants I1–I25

| Invariant | Description | Verification Method | Status |
| :--- | :--- | :--- | :---: |
| **I1** | 848 selected games per arm | Underlying per-game records | **PASS** |
| **I2** | Stable source_game_key per game | SHA-256 normalized UCI | **PASS** |
| **I3** | Deterministic game ordering | Ascending GameKey sort | **PASS** |
| **I4** | H2H lexicographic canonical-ID rule | Frozen Q2.3 selection logic | **PASS** |
| **I5** | Candidate plies 12..120 inclusive | Move loop range check | **PASS** |
| **I6** | candidate_position_id format | `source_game_key:ply` | **PASS** |
| **I7** | Q3/Q4 raw occurrence identity | Bit-for-bit set equality | **PASS** |
| **I8** | Q3/Q4 external occurrence identity | Bit-for-bit set equality | **PASS** |
| **I9** | Historical leakage frozen rules | Exact hash matching | **PASS** |
| **I10** | Blind leakage frozen rules | Exact hash matching | **PASS** |
| **I11** | Collision keys post-exclusion intersection | Exact set intersection | **PASS** |
| **I12** | Symmetric-Hash-Parity-v1 on collisions only | Non-collision flag check | **PASS** |
| **I13** | Non-collision origin arm ownership | Origin arm retention | **PASS** |
| **I14** | Single owner per collision key | Owner assignment count = 1 | **PASS** |
| **I15** | Repeated boards remain distinct occurrences | Position ID preservation | **PASS** |
| **I16** | No post-ownership transfer | Occurrence mapping invariant | **PASS** |
| **I17** | Final canonical sets disjoint | Key intersection = 0 | **PASS** |
| **I18** | Structural capacity counts occurrences | $|S_g|$ occurrence count | **PASS** |
| **I19** | Feasibility predicate $c \ge K$ | Evaluation of $c < K$ vs $c \ge K$ | **PASS** |
| **I20** | $\text{selected} = \text{feasible} + \text{infeasible}$ | $848 = 842 + 6$ for both arms | **PASS** |
| **I21** | Arm-B failures supported by records | Individual record audit | **PASS** |
| **I22** | Arm-A failures completely enumerated | Individual record audit | **PASS** |
| **I23** | Dependent summaries equal derived counts | Cross-artifact checksum | **PASS** |
| **I24** | Repeatability deterministic | Bit-for-bit run equality | **PASS** |
| **I25** | Zero teacher/training/eval execution | Subprocess & invocation audit | **PASS** |

---

## 6. Authoritative Artifact Checksums

| Artifact | Role | SHA-256 Digest |
| :--- | :--- | :--- |
| `docs/B3_OTB_2_Q4_PRELABEL_STRUCTURAL_FEASIBILITY_RECONCILIATION_SPEC.md` | New Reconciliation Spec | `COMPUTED_POST_WRITE` |
| `data/b3_otb_2/q4_prelabel_structural_reconciliation_audit.json` | New Reconciliation Audit Data | `COMPUTED_POST_WRITE` |
| `docs/B3_OTB_2_Q4_PRELABEL_STRUCTURAL_FEASIBILITY_RECONCILIATION_REPORT.md` | New Reconciliation Report | `COMPUTED_POST_WRITE` |
| `data/b3_otb_2/q4_arm_a_structural_selection.json` | Committed Selection (Arm A) | `19dbb52778b6758d017f6c0ab48c428901b4bdec457ff213cb09101feeef553f` |
| `data/b3_otb_2/q4_arm_b_structural_selection.json` | Committed Selection (Arm B) | `03e8f439ab491d0289e9e40403770a2709d8072118e3febe4b08735f35288eae` |
| `data/b3_otb_2/q4_candidate_capacity_audit.json` | Committed Capacity Audit | `ddbdb0bbe39efe010127066264546674ab372f8e3bb0f406d549f47e646c574c` |
| `data/b3_otb_2/q4_collision_ownership_audit.json` | Committed Collision Audit | `d9aab87028e2ba43748c67e3acff27aea0ad1e421069576153245bfaba79a499` |
| `data/b3_otb_2/q4_canonical_leakage_audit.json` | Committed Leakage Audit | `4a29d05df249b2d7e228437dc8aba3fa2958c88126d21d99559e77d22c9f371f` |
| `data/b3_otb_2/q4_repeatability_audit.json` | Committed Repeatability Audit | `0b4454a4b78cf4169f60b26eec52ea97793838fe666a598f280c2f78f6f6ce76` |
| `data/b3_otb_2/q4_execution_audit.json` | Committed Execution Audit | `9268f84a20e8c9769f4c077bd1286083cb767f5b142fa559808e519fb0ed1ee4` |
| `data/b3_otb_2/q4_prelabel_structural_feasibility_manifest.json` | Committed Feasibility Manifest | `310a9ab6ee1c8f3850fa44e74d4f64994bdef47bf7649ffcbf383cb842631f43` |
| `docs/B3_OTB_2_Q4_PRELABEL_STRUCTURAL_FEASIBILITY_AUDIT_REPORT.md` | Committed Audit Report | `52ebb36dad5844b3ab91fa1a0d6d277c0a904d4547e2a9ffa029219a235eb734` |

---

## 7. Governance Conclusion & Submission

* **EVIDENCE RECONCILIATION COMPLETE:** All underlying per-game records and occurrence streams have been reconciled without modifying frozen rules or design specifications.
* **AUDIT VERDICT:** Structural Feasibility remains **`FAIL-CLOSED`** due to 12 shortfalls (6 Arm A games, 6 Arm B games with $c < K$).
* **DOWNSTREAM GATE:** In strict adherence to Fail-Closed Governance, zero teacher labeling is authorized.
* **SUBMISSION:** This authoritative reconciliation record is submitted to GPT-1 for final audit ratification.
