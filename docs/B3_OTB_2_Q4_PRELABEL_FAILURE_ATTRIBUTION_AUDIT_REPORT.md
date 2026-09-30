# BOSON B3-OTB-2: Q4 Pre-Label Failure-Attribution Audit Report
**Read-Only Structural Root-Cause Analysis for the 12 Shortfall Games**  
**Audit Identifier:** `AUDIT-B3-OTB-2-Q4-PRELABEL-FAILURE-ATTRIBUTION`  
**Execution Timestamp:** 2026-09-30T13:23:07Z  
**Governing Checkpoint:** `9dd30796fba15e3c3cfb90906ecd4d82e12c6cd5`  
**Specification Reference:** `docs/B3_OTB_2_Q4_PRELABEL_FAILURE_ATTRIBUTION_AUDIT_SPEC.md`  

---

## 1. Executive Summary & Verdict

$$\mathbf{AUDIT\;STATUS:\;READ-ONLY\;ATTRIBUTION\;COMPLETE}$$
$$\mathbf{GOVERNANCE\;STATE:\;FAIL-CLOSED\;(12\;SHORTFALLS)}$$

This read-only failure-attribution audit evaluates the exact mechanical causes behind all 12 structural shortfalls in committed Q4 pre-label evidence. Across two identical, bit-for-bit verified evaluation passes:
* **Total Shortfall Games Analyzed:** Exactly **12 games** (6 in Arm A, 6 in Arm B).
* **Category B (`B_COLLISION_CAUSED`):** **10 games** (83.33% of failures: 6 in Arm A, 4 in Arm B).
  * These games were fully feasible before cross-arm collision resolution ($E \ge K$), but were pushed into shortfall exclusively by losing candidate positions to the opposite arm under `Symmetric-Hash-Parity-v1` ($C_{\text{other}} > 0$).
* **Category A (`A_INTRINSIC_PRECOLLISION`):** **2 games** (16.67% of failures: 0 in Arm A, 2 in Arm B).
  * These games had insufficient externally admissible positions ($E < K$) before collision resolution:
    1. **Caruana Game #119 (`1bfb936ea9f6f0ae...`):** $R = 8, H = 0, B = 0 \implies E = 8 < K = 12$ (intrinsic shortfall of -4; losing 2 collision candidates widened the shortfall to -6).
    2. **Ding Game #166 (`307587b4879d1d20...`):** $R = 6, H = 1, B = 0 \implies E = 5 < K = 11$ (intrinsic shortfall of -6; zero collision loss).
* **Category C (`C_OTHER_FROZEN_RULE_MECHANISM`):** **0 games**.

---

## 2. Comprehensive Per-Game Attribution Table (12 Games)

| Arm | Cohort Player | Overall Rank | Player Rank | Stable Source Game Key | Assigned $K$ | Raw $R$ | Hist $H$ | Blind $B$ | Ext $E$ | Non-Col $N$ | Col Total $C$ | Col Self $C_{\text{self}}$ | Col Other $C_{\text{other}}$ | Final Cap $F$ | Pre Shortfall | Final Shortfall | Col Loss | Primary Classification | Secondary Collision Effect |
| :---: | :--- | :---: | :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- | :--- |
| ARM_A | CARLSEN_MAGNUS | 3 | 3 | `00bab15ed5b14c9a...` | 12 | 16 | 1 | 0 | 15 | 0 | 15 | 9 | 6 | 9 | 0 | 3 | 6 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_A | CARLSEN_MAGNUS | 20 | 20 | `0509f66abf9b5e2b...` | 12 | 30 | 0 | 0 | 30 | 0 | 30 | 6 | 24 | 6 | 0 | 6 | 24 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_A | CARLSEN_MAGNUS | 66 | 66 | `13f2c8c0e26ff64c...` | 12 | 25 | 2 | 0 | 23 | 0 | 23 | 11 | 12 | 11 | 0 | 1 | 12 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_A | CARLSEN_MAGNUS | 140 | 140 | `2ac07be82418b825...` | 12 | 27 | 0 | 0 | 27 | 0 | 27 | 11 | 16 | 11 | 0 | 1 | 16 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_A | CARLSEN_MAGNUS | 183 | 183 | `3760bb39b56d4e1c...` | 12 | 21 | 0 | 0 | 21 | 0 | 21 | 6 | 15 | 6 | 0 | 6 | 15 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_A | CARLSEN_MAGNUS | 666 | 666 | `c931ba0c235ac128...` | 11 | 27 | 16 | 0 | 11 | 3 | 8 | 5 | 3 | 8 | 0 | 3 | 3 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_B | CARLSEN_MAGNUS | 173 | 3 | `00bab15ed5b14c9a...` | 12 | 16 | 1 | 0 | 15 | 0 | 15 | 6 | 9 | 6 | 0 | 6 | 9 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_B | CARLSEN_MAGNUS | 183 | 13 | `046af20d5a74cdeb...` | 12 | 35 | 0 | 0 | 35 | 0 | 35 | 11 | 24 | 11 | 0 | 1 | 24 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_B | CARLSEN_MAGNUS | 197 | 27 | `081b6da9a5ade734...` | 12 | 27 | 2 | 0 | 25 | 0 | 25 | 11 | 14 | 11 | 0 | 1 | 14 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_B | CARLSEN_MAGNUS | 238 | 68 | `168bc79f06f6ff31...` | 12 | 27 | 1 | 0 | 26 | 0 | 26 | 11 | 15 | 11 | 0 | 1 | 15 | `B_COLLISION_CAUSED` | `COLLISION_CREATED_SHORTFALL` |
| ARM_B | CARUANA_FABIANO | 459 | 119 | `1bfb936ea9f6f0ae...` | 12 | 8 | 0 | 0 | 8 | 6 | 2 | 0 | 2 | 6 | 4 | 6 | 2 | `A_INTRINSIC_PRECOLLISION` | `COLLISION_WORSENED_EXISTING_SHORTFALL` |
| ARM_B | DING_LIREN | 676 | 166 | `307587b4879d1d20...` | 11 | 6 | 1 | 0 | 5 | 5 | 0 | 0 | 0 | 5 | 6 | 6 | 0 | `A_INTRINSIC_PRECOLLISION` | `NO_COLLISION_LOSS` |

---

## 3. Deep-Dive Attribution by Arm

### 3.1 Arm A Shortfalls (6 games) — 100% Collision-Caused
Every single failed game in Arm A was structurally feasible prior to cross-arm collision resolution ($E \ge K$). In all 6 cases, cross-arm opening collisions assigned to Arm B under `Symmetric-Hash-Parity-v1` reduced candidate capacity below $K$:
* **Rank 3 (`00bab15ed5b14c9a...`):** $E = 15 \ge 12$. Collision loss $= 6 \implies F = 9 < 12$ (`B_COLLISION_CAUSED`, shortfall -3).
* **Rank 20 (`0509f66abf9b5e2b...`):** $E = 30 \ge 12$. Collision loss $= 24 \implies F = 6 < 12$ (`B_COLLISION_CAUSED`, shortfall -6).
* **Rank 66 (`13f2c8c0e26ff64c...`):** $E = 23 \ge 12$. Collision loss $= 12 \implies F = 11 < 12$ (`B_COLLISION_CAUSED`, shortfall -1).
* **Rank 140 (`2ac07be82418b825...`):** $E = 27 \ge 12$. Collision loss $= 16 \implies F = 11 < 12$ (`B_COLLISION_CAUSED`, shortfall -1).
* **Rank 183 (`3760bb39b56d4e1c...`):** $E = 21 \ge 12$. Collision loss $= 15 \implies F = 6 < 12$ (`B_COLLISION_CAUSED`, shortfall -6).
* **Rank 666 (`c931ba0c235ac128...`):** $E = 11 \ge 11$. Collision loss $= 3 \implies F = 8 < 11$ (`B_COLLISION_CAUSED`, shortfall -3).

### 3.2 Arm B Shortfalls (6 games) — 4 Collision-Caused, 2 Intrinsic Shortfalls
* **Carlsen Rank 173 (Player Rank 3):** $E = 15 \ge 12$. Collision loss $= 9 \implies F = 6 < 12$ (`B_COLLISION_CAUSED`, shortfall -6).
* **Carlsen Rank 183 (Player Rank 13):** $E = 35 \ge 12$. Collision loss $= 24 \implies F = 11 < 12$ (`B_COLLISION_CAUSED`, shortfall -1).
* **Carlsen Rank 197 (Player Rank 27):** $E = 25 \ge 12$. Collision loss $= 14 \implies F = 11 < 12$ (`B_COLLISION_CAUSED`, shortfall -1).
* **Carlsen Rank 238 (Player Rank 68):** $E = 26 \ge 12$. Collision loss $= 15 \implies F = 11 < 12$ (`B_COLLISION_CAUSED`, shortfall -1).
* **Caruana Rank 459 (Player Rank 119):** $R = 8, H = 0, B = 0 \implies E = 8 < 12$. Intrinsic pre-collision shortfall of -4. Collision loss of 2 widened shortfall to -6 (`A_INTRINSIC_PRECOLLISION`, `COLLISION_WORSENED_EXISTING_SHORTFALL`).
* **Ding Rank 676 (Player Rank 166):** $R = 6, H = 1, B = 0 \implies E = 5 < 11$. Intrinsic pre-collision shortfall of -6. Collision loss $= 0$ (`A_INTRINSIC_PRECOLLISION`, `NO_COLLISION_LOSS`).

---

## 4. Global Balance & Flow Reconciliation

| Accounting Dimension | Arm A | Arm B | Cross-Arm Invariant |
| :--- | :---: | :---: | :---: |
| **Total Selected Games** | 848 | 848 | Invariant I1 satisfied |
| **Raw Candidate Occurrences ($R$)** | 65,464 | 63,272 | Invariant I7 satisfied |
| **Historical Exclusions ($H$)** | 809 | 906 | Frozen Historical Datasets |
| **Blind Exclusions ($B$)** | 6 | 3 | Frozen Blind Benchmark |
| **Externally Admissible ($E$)** | 64,649 | 62,363 | $E = R - H - B$ satisfied |
| **Non-Collision Occurrences ($N$)** | 48,908 | 46,634 | Retained by origin arm |
| **Collision Occurrences ($C$)** | 15,741 | 15,729 | $|\mathcal{K}_{\text{collision}}| = 14,591$ |
| **Collision Owned ($C_{\text{self}}$)** | 7,949 | 7,804 | Allocated via `Symmetric-Hash-Parity-v1` |
| **Collision Lost ($C_{\text{other}}$)** | 7,792 | 7,925 | Allocated to opposing arm |
| **Final Structural Capacity ($F$)** | **56,857** | **54,438** | $F = N + C_{\text{self}} = E - C_{\text{other}}$ |
| **Assigned Target Quota** | 9,965 | 9,965 | Quota exceeded by +46,892 / +44,473 |
| **Feasible Games ($c \ge K$)** | 842 (99.29%) | 842 (99.29%) | 99.29% game-level feasibility |
| **Shortfall Games ($c < K$)** | **6 (0.71%)** | **6 (0.71%)** | Total = 12 Shortfall Games |

---

## 5. Verification of All Invariants

| Invariant Check | Required Condition | Observed Value | Verification Result |
| :--- | :--- | :--- | :---: |
| **Per-Game Decomposition** | $\forall g: R = E + H + B$ | 12 / 12 verified | **PASS** |
| **Per-Game Collision Flow** | $\forall g: E = N + C \land C = C_{\text{self}} + C_{\text{other}}$ | 12 / 12 verified | **PASS** |
| **Per-Game Final Capacity** | $\forall g: F = N + C_{\text{self}} = E - C_{\text{other}}$ | 12 / 12 verified | **PASS** |
| **Shortfall Consistency** | $\forall g: \text{final\_shortfall} = \max(0, K - F) > 0$ | 12 / 12 verified | **PASS** |
| **Global Raw Sum** | $R_{\text{total}} = E_{\text{total}} + H_{\text{total}} + B_{\text{total}}$ | Exact match both arms | **PASS** |
| **Global External Sum** | $E_{\text{total}} = N_{\text{total}} + C_{\text{total}}$ | Exact match both arms | **PASS** |
| **Global Capacity Sum** | $F_{\text{total}} = N_{\text{total}} + C_{\text{self,total}}$ | Exact match both arms | **PASS** |
| **Cross-Arm Disjointness** | $\mathcal{K}_{A,\text{final}} \cap \mathcal{K}_{B,\text{final}} = \emptyset$ | 0 overlapping keys | **PASS** |
| **Deterministic Repeatability** | Pass 1 vs Pass 2 bit-for-bit | 100% Identical | **PASS** |
| **Zero Downstream Execution** | Stockfish / Teacher / Training $= 0$ | 0 invocations | **PASS** |

---

## 6. Newly Created Evidence Artifacts & Checksums

| Artifact Path | SHA-256 Digest | Byte Size |
| :--- | :--- | :---: |
| `docs/B3_OTB_2_Q4_PRELABEL_FAILURE_ATTRIBUTION_AUDIT_SPEC.md` | `5d8c4344cf391d28582f4809300b5b75a11a2fdfdc58b39f53619fb7997658c6` | 3403 bytes |
| `data/b3_otb_2/q4_failure_attribution_audit.json` | `01e68041ffcd9d350e2c101466d32dff70319cda8782c07d9192d8d86ddae12e` | 15945 bytes |
| `docs/B3_OTB_2_Q4_PRELABEL_FAILURE_ATTRIBUTION_AUDIT_REPORT.md` | Self (Audit Report Document) | Dynamic |
| `data/b3_otb_2/q4_failure_attribution_execution_audit.json` | `d018d051ce7fc7dc07a7095a93419c741bd5826e6c76a0e4afbb65cd5e895d8b` | 1120 bytes |

---

## 7. Epistemic Boundary & Next Steps

* **READ-ONLY ATTRIBUTION COMPLETE:** The empirical causes of all 12 shortfalls are uniquely and rigorously identified without ambiguity.
* **REMAINING SHORTFALLS:** 10 games fail due to cross-arm collision loss (`B_COLLISION_CAUSED`), and 2 games fail due to insufficient length/admissible candidates (`A_INTRINSIC_PRECOLLISION`).
* **GOVERNANCE STATUS:** The audit terminates at the read-only attribution boundary with status **`READ_ONLY_ATTRIBUTION_COMPLETE`**.
* **NO CODE / DATA MODIFICATION:** No existing Q4 artifacts are altered; no git commit or push is executed. All findings are submitted for architectural review by GPT-1.
