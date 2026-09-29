# BOSON B3-OTB-2: Q4 Pre-Label Structural Feasibility Audit Specification
**Execution Protocol & Governance Boundaries**  
**Specification Identifier:** `SPEC-B3-OTB-2-Q4-PRELABEL-STRUCTURAL-FEASIBILITY-AUDIT`  
**Parent Document:** `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` (Ratified by GPT-1)  
**Status:** `AUDIT COMPLETE — RESULT: FAIL-CLOSED`  

---

> [!IMPORTANT]
> **ABSOLUTE EPISTEMIC BOUNDARY:**  
> *"Q4 structural feasibility establishes only that the amended candidate construction has sufficient pre-label structural capacity under the frozen collision and game-level rules. Finite-CP feasibility remains unknown until authorized teacher labeling."*

---

## 1. Audit Mandate & Scope
This audit executes the authorized pre-label structural feasibility verification for experiment B3-OTB-2 under Q4 Draft 4 rules.
The audit is strictly restricted to:
1. Verifying deterministic reproduction of frozen source-game selections for Arm A (848 games) and Arm B (848 games).
2. Generating legal candidate occurrences for plies 12..120 in strictly ascending order without intra-game deduplication (Q4-I14).
3. Applying external historical and blind GameKey exclusions.
4. Detecting cross-arm canonical collisions ($\mathcal{K}_{\text{collision}}$).
5. Applying `Symmetric-Hash-Parity-v1` exclusively to true collision keys (Q4-I15).
6. Preserving non-collision keys in their originating arm without hash partitioning.
7. Measuring pre-label structural candidate capacity $|S_g|$ against assigned game quotas $K \in \{11, 12\}$.
8. Evaluating Fail-Closed Governance: requiring $\text{structural\_capacity}(g) \ge K(g)$ for every single selected game.
9. Proving bit-for-bit repeatability across two identical audit executions.

---

## 2. Parent Lineage & Immutable Controls

| Control Asset | Path | SHA-256 Digest | State |
| :--- | :--- | :--- | :---: |
| **Q2.3 Specification** | `docs/B3_OTB_2_SUPERSEDING_DESIGN_SPEC_Q2_3_DRAFT.md` | `08f7ece7cb47967890690b7d2c007973fe2c2a25df4df6470feec02fc23438af` | `FROZEN` |
| **Q2.3 Manifest** | `data/b3_otb_2/superseding_design_manifest_q2_3_draft.json` | `b3c06d2032b79e5dbf96764699347b104100d5d1c6d4165b2e894e630e270b95` | `FROZEN` |
| **Q4 Draft 4 Spec** | `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` | `e08e711d1a87abe385ec928f90a12777f774329b105556e81a55a424518ee0fc` | `RATIFIED PARENT` |
| **Q4 Draft 4 Manifest** | `data/b3_otb_2/q4_design_amendment_manifest_draft_4.json` | `d1a04ad06103f91679ea2cb8d6527729f4f348feae2a8ea5440544e084c29762` | `RATIFIED PARENT` |
| **Frozen Blind Benchmark** | `data/b3a_blind_test/blind_positions_labeled.json` | `d8934c5c90d93bf6a8885d56f5d2c24f247466c9b8d33a45ccf5ea3865a5dc82` | `IMMUTABLE` |
| **Production Model** | `models/boson-v2.nnue` | `ef3386104547109445a47257c85afd99beef3cadbf7766566244e76a040dae92` | `IMMUTABLE` |

---

## 3. Structural Candidate Formulation
* **Candidate Occurrence:** Identified by `candidate_position_id = source_game_key + ":" + ply`.
* **Within-Game Non-Deduplication (Q4-I14):** Repeated `canonical_board_key` occurrences at different plies within the same game remain distinct candidates.
* **Collision Ownership (Q4-I15):** All candidate occurrences carrying collision key $k \in \mathcal{K}_{\text{collision}}$ inherit $\text{Owner}(k)$ via `Symmetric-Hash-Parity-v1`.
* **Structural Capacity:** $\text{structural\_capacity}(g) = |S_g|$ (count of candidate occurrences).
* **Structural Feasibility Threshold:** $\forall g: \text{structural\_capacity}(g) \ge \text{assigned\_K}(g)$.

---

## 4. Zero Downstream Execution Boundary
The audit strictly asserts:
* `stockfish_invocations = 0`
* `teacher_harness_invocations = 0`
* `teacher_label_records_created = 0`
* `training_invocations = 0`
* `checkpoint_created = false`
* `blind_evaluation_invocations = 0`
* `calibration_invocations = 0`
* `production_model_modified = false`
