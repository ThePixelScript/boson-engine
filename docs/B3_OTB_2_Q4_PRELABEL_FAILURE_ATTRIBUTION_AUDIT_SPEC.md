# BOSON B3-OTB-2: Q4 Pre-Label Failure-Attribution Audit Specification
**Protocol, Causality Rules, and Analytical Attribution Framework**  
**Specification Identifier:** `SPEC-B3-OTB-2-Q4-PRELABEL-FAILURE-ATTRIBUTION`  
**Governing Checkpoint:** `9dd30796fba15e3c3cfb90906ecd4d82e12c6cd5`  
**Parent Document:** `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` (Ratified by GPT-1)  
**Status:** `AUDIT COMPLETE — READ-ONLY ANALYSIS`  

---

> [!IMPORTANT]
> **ABSOLUTE EPISTEMIC & OPERATIONAL BOUNDARIES:**  
> 1. This audit is **strictly read-only**. It inspects already-committed Q4 records and frozen parents.
> 2. No existing source data or audit data is modified.
> 3. Zero Stockfish, teacher harness, training, evaluation, calibration, or model modification.
> 4. Do not commit or push in this stage; results are submitted for GPT-1 review.

---

## 1. Audit Mandate & Scope

The mandate of this audit is to analyze the 12 selected games satisfying $\text{structural\_capacity}(g) < K(g)$ and establish the exact causal mechanism for each shortfall:
* **Category A (`A_INTRINSIC_PRECOLLISION`):** The game had insufficient externally admissible positions ($E < K$) *prior to* cross-arm canonical collision resolution.
* **Category B (`B_COLLISION_CAUSED`):** The game had sufficient externally admissible positions ($E \ge K$), but lost positions to the opposite arm via `Symmetric-Hash-Parity-v1` ($C_{\text{other}} > 0$), resulting in $F < K$.
* **Category C (`C_OTHER_FROZEN_RULE_MECHANISM`):** Another frozen-rule mechanism demonstrably supported by evidence (invoked only if neither A nor B holds).

---

## 2. Frozen Parent Lineage & Immutable Anchors

| Artifact | Canonical Path | SHA-256 Digest | Governance Role |
| :--- | :--- | :--- | :---: |
| **Q4 Reconciliation Commit** | Git Commit `9dd3079` | `9dd30796fba15e3c3cfb90906ecd4d82e12c6cd5` | Authoritative Baseline |
| **Q2.3 Specification** | `docs/B3_OTB_2_SUPERSEDING_DESIGN_SPEC_Q2_3_DRAFT.md` | `08f7ece7cb47967890690b7d2c007973fe2c2a25df4df6470feec02fc23438af` | Frozen Design Authority |
| **Q4 Draft 4 Spec** | `docs/B3_OTB_2_Q4_DESIGN_AMENDMENT_DRAFT_4.md` | `e08e711d1a87abe385ec928f90a12777f774329b105556e81a55a424518ee0fc` | Ratified Design Spec |
| **Q4 Draft 4 Manifest** | `data/b3_otb_2/q4_design_amendment_manifest_draft_4.json` | `d1a04ad06103f91679ea2cb8d6527729f4f348feae2a8ea5440544e084c29762` | Ratified Amendment Manifest |

---

## 3. Mathematical Identities & Causality Predicates

For each game $g$:
* $R = E + H + B$ (with $H \cap B = \emptyset$)
* $E = N + C$
* $C = C_{\text{self}} + C_{\text{other}}$
* $F = N + C_{\text{self}} = E - C_{\text{other}}$
* $\text{pre\_shortfall} = \max(0, K - E)$
* $\text{final\_shortfall} = \max(0, K - F)$
* $\text{collision\_loss} = C_{\text{other}}$
* $\text{collision\_shortfall\_increase} = \text{final\_shortfall} - \text{pre\_shortfall}$

### Causality Rules:
1. **Primary Classification:**
   * `A_INTRINSIC_PRECOLLISION`: $E < K$
   * `B_COLLISION_CAUSED`: $E \ge K \land F < K \land C_{\text{other}} > 0$
   * `C_OTHER_FROZEN_RULE_MECHANISM`: $F < K \land \neg A \land \neg B$
2. **Secondary Collision Effect:**
   * `NO_COLLISION_LOSS`: $C_{\text{other}} = 0$
   * `COLLISION_WORSENED_EXISTING_SHORTFALL`: $E < K \land C_{\text{other}} > 0$
   * `COLLISION_CREATED_SHORTFALL`: $E \ge K \land F < K$
