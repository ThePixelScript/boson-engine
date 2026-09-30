# BOSON B3-OTB-2: Q4 Pre-Label Structural Feasibility Reconciliation Specification
**Protocol, Invariants, and Authoritative Underlying Truth Verification**  
**Specification Identifier:** `SPEC-B3-OTB-2-Q4-PRELABEL-STRUCTURAL-FEASIBILITY-RECONCILIATION`  
**Parent Document:** `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` (Ratified by GPT-1)  
**Governing Checkpoint:** `aa07ae8c89a01fa6e84f92e01eacb275d4c42c1e`  
**Status:** `RECONCILIATION COMPLETE — STATUS: FAIL-CLOSED (12 SHORTFALLS)`  

---

> [!IMPORTANT]
> **ABSOLUTE EPISTEMIC BOUNDARY:**  
> *"This reconciliation stage operates strictly offline on existing authoritative underlying records. It does NOT authorize teacher labeling, Stockfish execution, dataset construction, training, evaluation, calibration, or model modification. Its sole mandate is to reconcile and prove the exact underlying record truth regarding Arm-B failure counts and Q3 vs Q4 candidate accounting."*

---

## 1. Reconciliation Mandate & Scope

Following GPT-1's conditional review of committed Q4 structural audit checkpoint `aa07ae8c89a01fa6e84f92e01eacb275d4c42c1e`, this specification defines the evidence reconciliation protocol addressing two specific issues:
1. **Reconciliation Issue 1 (Arm-B Failure Count):** Derive the exact feasible and infeasible game counts from underlying per-game records under the predicate $	ext{infeasible}(g) \iff c(g) < K(g)$, evaluate the seven named records from GPT-1's review individually, and verify that $	ext{selected} = 	ext{feasible} + 	ext{infeasible}$.
2. **Reconciliation Issue 2 (Q3 vs. Q4 Candidate Accounting):** Compare candidate occurrence identity sets ($12 \le 	ext{ply} \le 120$, identified by `source_game_key:ply`) between frozen Q3 and Q4 for both raw and external streams, verify exact set equivalence ($	ext{Q3\_RAW} == 	ext{Q4\_RAW}$ and $	ext{Q3\_EXT} == 	ext{Q4\_EXT}$), and account for external historical and blind exclusions without double-counting.

---

## 2. Frozen Parent Lineage & Immutable Anchors

| Artifact | Canonical Path | SHA-256 Digest | Governance Role |
| :--- | :--- | :--- | :---: |
| **Q4 Checkpoint** | Git Commit `aa07ae8` | `aa07ae8c89a01fa6e84f92e01eacb275d4c42c1e` | Frozen Parent Commit |
| **Q2.3 Specification** | `docs/B3_OTB_2_SUPERSEDING_DESIGN_SPEC_Q2_3_DRAFT.md` | `08f7ece7cb47967890690b7d2c007973fe2c2a25df4df6470feec02fc23438af` | Frozen Design Authority |
| **Q2.3 Manifest** | `data/b3_otb_2/superseding_design_manifest_q2_3_draft.json` | `b3c06d2032b79e5dbf96764699347b104100d5d1c6d4165b2e894e630e270b95` | Frozen Lineage Manifest |
| **Q4 Draft 4 Spec** | `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` | `e08e711d1a87abe385ec928f90a12777f774329b105556e81a55a424518ee0fc` | Ratified Design Spec |
| **Q4 Draft 4 Manifest** | `data/b3_otb_2/q4_design_amendment_manifest_draft_4.json` | `d1a04ad06103f91679ea2cb8d6527729f4f348feae2a8ea5440544e084c29762` | Ratified Amendment Manifest |
| **Blind Benchmark** | `data/b3a_blind_test/blind_positions_labeled.json` | `d8934c5c90d93bf6a8885d56f5d2c24f247466c9b8d33a45ccf5ea3865a5dc82` | Immutable Blind Baseline |
| **Production Model** | `models/boson-v2.nnue` | `ef3386104547109445a47257c85afd99beef3cadbf7766566244e76a040dae92` | Immutable Frozen Weights |

---

## 3. The 25 Mandatory Invariants

* **I1.** 848 selected games per arm ($N_A = 848, N_B = 848$).
* **I2.** Every selected game has exactly one stable `source_game_key` derived via SHA-256 of normalized UCI moves.
* **I3.** Game ordering remains the frozen deterministic `source_game_key` ascending ordering.
* **I4.** H2H ownership remains exactly the frozen lexicographic canonical-ID rule.
* **I5.** Candidate plies are strictly $12 \le 	ext{ply} \le 120$.
* **I6.** `candidate_position_id` is formatted strictly as `<source_game_key>:<decimal_ply>`.
* **I7.** Q3 and Q4 raw candidate occurrence streams are reconciled and proved identical ($	ext{Q3\_RAW} == 	ext{Q4\_RAW}$).
* **I8.** Q3 and Q4 external admissible occurrence streams are reconciled and proved identical ($	ext{Q3\_EXT} == 	ext{Q4\_EXT}$).
* **I9.** Historical dataset leakage exclusions follow frozen rules across `SYN-20K`, `REAL-20K`, `STRONG-OTB-01`, and `REAL-9965`.
* **I10.** Blind benchmark leakage exclusions follow frozen rules against `BLIND-TEST-LICHESS-2026-08-5K`.
* **I11.** Collision keys are exactly $\mathcal{K}_{	ext{collision}} = \mathcal{K}_A \cap \mathcal{K}_B$ of post-exclusion canonical board keys.
* **I12.** `Symmetric-Hash-Parity-v1` is applied strictly and exclusively to collision keys $\mathcal{K}_{	ext{collision}}$.
* **I13.** Non-collision candidate occurrences retain their originating arm ownership without hash partitioning.
* **I14.** Every occurrence of a collision key inherits the exact same assigned owner across all games and arms.
* **I15.** Repeated canonical boards at different plies within the same game remain distinct candidate occurrences (Q4-I14).
* **I16.** No candidate occurrence is transferred or reallocated after ownership assignment.
* **I17.** Final Arm-A and Arm-B canonical board key sets are strictly disjoint ($\mathcal{K}_{A,	ext{final}} \cap \mathcal{K}_{B,	ext{final}} = \emptyset$).
* **I18.** Structural candidate capacity counts candidate occurrences $|S_g|$, not finite teacher evaluations.
* **I19.** For every game $g$, $	ext{feasible}(g) \iff c(g) \ge K(g)$, and $	ext{infeasible}(g) \iff c(g) < K(g)$.
* **I20.** $	ext{selected\_games} = 	ext{feasible\_games} + 	ext{infeasible\_games}$ holds identically for both arms ($848 = 842 + 6$).
* **I21.** Every listed Arm-B shortfall game is directly and individually supported by its underlying record.
* **I22.** All Arm-A shortfall games are completely and individually enumerated.
* **I23.** Every summary counter in all dependent artifacts matches the underlying record-derived count.
* **I24.** Repeatability remains 100% bit-for-bit deterministic across independent runs.
* **I25.** Zero teacher harness, Stockfish, training, evaluation, calibration, or model modification executed.
