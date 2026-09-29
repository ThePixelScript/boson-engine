# BOSON B3-OTB-2: Q4 Pre-Label Structural Feasibility Audit Report
**Execution Findings & Audit Verdict**  
**Audit Identifier:** `AUDIT-B3-OTB-2-Q4-PRELABEL-STRUCTURAL-FEASIBILITY`  
**Execution Timestamp:** 2026-09-29T19:46:02Z  
**Governing Design Specification:** `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` (Ratified by GPT-1)  

---

## 1. Executive Summary & Verdict

$$\mathbf{AUDIT\;STATUS:\;FAIL-CLOSED}$$

Under the ratified Q4 Draft 4 protocol (Final-Admitted Canonical Disjointness via `Symmetric-Hash-Parity-v1`, `SAME_GAME_ONLY` refill, and non-deduplication of within-game repeated canonical boards), the structural audit completed two identical, bit-for-bit verified passes.

### Headline Findings:
1. **Total Pre-Label Structural Capacity:**
   * **Arm A:** **56,857** candidate occurrences across 848 games (target: 9,965; surplus: +46,892).
   * **Arm B:** **54,438** candidate occurrences across 848 games (target: 9,965; surplus: +44,473).
2. **Canonical Collision Set:**
   * Distinct canonical keys in Arm A: **62,456**
   * Distinct canonical keys in Arm B: **59,858**
   * Total cross-arm collision keys ($|\mathcal{K}_{\text{collision}}|$): **14,591**
   * Arm-A Owned via `Symmetric-Hash-Parity-v1`: **7,304** (50.06%)
   * Arm-B Owned via `Symmetric-Hash-Parity-v1`: **7,287** (49.94%)
3. **Cross-Arm Structural Canonical Disjointness:**
   * $\text{Keys}(S_A) \cap \text{Keys}(S_B) = \mathbf{0}$ (Strict Canonical Disjointness Verified).
4. **Within-Game Duplicate Preservation (Q4-I14):**
   * Arm A: **1,061** duplicate canonical occurrences preserved.
   * Arm B: **1,137** duplicate canonical occurrences preserved.
5. **Structural Feasibility Threshold Evaluation:**
   * **Arm A Failed Games:** **6 games** with $\text{structural\_capacity}(g) < K$.
   * **Arm B Failed Games:** **6 games** with $\text{structural\_capacity}(g) < K$ (4 Carlsen, 1 Caruana, 1 Ding).
   * **Total Shortfall Games:** **12 games**.
   * Under Section 15 Fail-Closed Governance, which strictly mandates $\text{structural\_capacity}(g) \ge K(g)$ for 100% of assigned source games without cross-game borrowing or game substitution, the audit terminates with status **`FAIL-CLOSED`**.

---

## 2. Failed Games Breakdown (Structural Shortfalls)

Under `SAME_GAME_ONLY` refill, the following 12 games have fewer structural candidate occurrences than assigned $K$:

### Arm A Shortfalls (6 games):
| Rank | Source Game Key | Assigned $K$ | Final Structural Capacity | Shortfall | White | Black | Total Plies |
| :---: | :--- | :---: | :---: | :---: | :--- | :--- | :---: |
| 3 | `00bab15ed5b14c9a...` | 12 | 9 | -3 | Ding Liren | Carlsen,M | 27 |
| 20 | `0509f66abf9b5e2b...` | 12 | 6 | -6 | Carlsen,M | Nakamura,Hi | 41 |
| 66 | `13f2c8c0e26ff64c...` | 12 | 11 | -1 | Rapport,R | Carlsen,M | 36 |
| 140 | `2ac07be82418b825...` | 12 | 11 | -1 | Carlsen,M | Dubov,Daniil | 38 |
| 183 | `3760bb39b56d4e1c...` | 12 | 6 | -6 | Carlsen,M | Anand,V | 32 |
| 666 | `c931ba0c235ac128...` | 11 | 8 | -3 | Radjabov,T | Carlsen,M | 38 |

### Arm B Shortfalls (6 games):
| Overall Rank | Player | Player Rank | Source Game Key | Assigned $K$ | Final Structural Capacity | Shortfall | White | Black | Total Plies |
| :---: | :--- | :---: | :--- | :---: | :---: | :---: | :--- | :--- | :---: |
| 173 | CARLSEN_MAGNUS | 3 | `00bab15ed5b14c9a...` | 12 | 6 | -6 | Ding Liren | Carlsen,M | 27 |
| 183 | CARLSEN_MAGNUS | 13 | `046af20d5a74cdeb...` | 12 | 11 | -1 | Giri,A | Carlsen,M | 46 |
| 197 | CARLSEN_MAGNUS | 27 | `081b6da9a5ade734...` | 12 | 11 | -1 | Topalov,V | Carlsen,M | 38 |
| 238 | CARLSEN_MAGNUS | 68 | `168bc79f06f6ff31...` | 12 | 11 | -1 | Karjakin,Sergey | Carlsen,M | 38 |
| 459 | CARUANA_FABIANO | 119 | `1bfb936ea9f6f0ae...` | 12 | 6 | -6 | Ding Liren | Caruana,F | 19 |
| 676 | DING_LIREN | 166 | `307587b4879d1d20...` | 11 | 5 | -6 | Ding Liren | So,W | 17 |

---

## 3. Arm B Cohort Structural Capacities

| Player Cohort | Assigned Games | Assigned Target Positions | Raw Candidates | Ext. Exclusions | Collision Occurrences | Owned Occurrences | Lost to Arm A | Total Structural Capacity | Failed Games ($c < K$) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **ANAND_VISWANATHAN** | 170 | 1,993 | 11,759 | 204 | 2,034 | 1,003 | 1,031 | **10,524** | 0 |
| **CARLSEN_MAGNUS** | 170 | 1,993 | 12,849 | 162 | 12,687 | 6,336 | 6,351 | **6,336** | 4 |
| **CARUANA_FABIANO** | 170 | 1,993 | 13,316 | 198 | 309 | 142 | 167 | **12,951** | 1 |
| **DING_LIREN** | 169 | 1,993 | 12,564 | 151 | 346 | 156 | 190 | **12,223** | 1 |
| **NAKAMURA_HIKARU** | 169 | 1,993 | 12,784 | 194 | 353 | 167 | 186 | **12,404** | 0 |
| **Arm B Total** | **848** | **9,965** | **63,272** | **909** | **15,729** | **7,804** | **7,925** | **54,438** | **6** |

*Note on Player Quotas:* Every single player comfortably exceeds the cohort aggregate quota of 1,993 positions in structural capacity (minimum player capacity is Carlsen at 6,336). However, the strict per-game requirement ($c \ge K$) fails for 6 individual short games.

---

## 4. Comparison: Q3 vs. Q4 Feasibility Evidence

| Metric | Q3 Audit (Superseded $R_A$) | Q4 Audit (Amended Parity) | Impact of Q4 Amendment |
| :--- | :---: | :---: | :--- |
| **Arm A Structural Capacity** | 64,649 | 56,857 | -7,792 (Allocated to Arm B) |
| **Arm B Structural Capacity** | 46,634 | 54,438 | +7,804 (Transferred to Arm B) |
| **Arm B Failed Games** | **197 games** | **6 games** | **191 games rescued** (-97.0% failure reduction) |
| **Carlsen Arm-B Failures** | 170 games | 4 games | 166 games rescued |
| **Anand H2H Failures** | 24 games | **0 games** | 24 games rescued (100% resolved) |
| **Arm A Failed Games** | 0 games | 6 games | +6 games (opening collisions assigned to Arm B) |
| **Cross-Arm Overlap** | 0 | 0 | Invariant maintained ($C_A \cap C_B = \emptyset$) |
| **Repeatability** | 100% | 100% | Bit-for-bit deterministic |
| **Verdict** | **FAIL-CLOSED** | **FAIL-CLOSED** | Maintained under zero-tolerance governance |

---

## 5. Epistemic Boundary & Next Steps
* **EPISTEMIC BOUNDARY:** This audit establishes structural candidate capacity only. Finite-CP feasibility remains unknown until authorized teacher labeling.
* **DOWNSTREAM ASSERTION:** Zero teacher evaluations, zero training runs, zero blind evaluations executed.
* **STATUS:** The execution agent halts at the structural audit boundary. All findings are submitted for architectural review by GPT-1 and GPT-2.
