# BOSON — Search Heuristics Architecture Map

This document establishes the interaction rules, sub-system ownership boundaries, and core design philosophies governing Boson's lookahead optimization ecosystem.

## 🗺 Heuristic Interaction Ecosystem Matrix

| Search Heuristic Layer | System Owner | Primary Consumer | Computational Objective | Pruning vs. Ordering Strategy | Interaction Impact Dependency |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Transposition Table (TT)** | `Search` | `Search` / `MoveOrderer` | Cache verified historical subtree bounds | Exact Cutoff / Immediate Path Sorting | Bypasses Alpha-Beta workload entirely upon a depth hit. |
| **Static Exchange Eval (SEE)** | `SEE` Subsystem | `MoveOrderer` | Calculate material balance values locally | Isolates winning tactical trades from traps | Protects LMR from reducing losing capture traps. |
| **Aspiration Windows** | `Search` | `Search` | Narrow search scope using previous depth scores | Alpha-Beta Window Reduction Optimization | Forces narrow scout windows; re-searches if boundaries fail. |
| **Null Move Pruning (NMP)** | `Search` | `Search` | Verify if passing turn preserves high score | Subtree Pruning | Safely cuts branches early in non-check, high-material nodes. |
| **Late Move Reductions (LMR)** | `Alpha-Beta` | `Search` | Spend fewer plies on low-probability paths | Depth Reduction (Soft Filter) | Highly dependent on MoveOrderer efficiency to prevent tactical leaks. |
| **Killer Moves** | `Search` | `MoveOrderer` | Track recent non-capture refutations at ply | Quiet Move Priority Elevation | Sits directly above CMH to catch threat replies. |
| **Counter-Move History** | `Search` | `MoveOrderer` | Track immediate contextual refutation replies | Quiet Move Priority Elevation | Slots between Killers and Global History using an L1-cache matrix. |
| **Global History Table** | `Search` | `MoveOrderer` | Measure long-term quiet move success | Long-Term Statistical Sorting | Scaled by an aging guard policy to prevent saturation. |