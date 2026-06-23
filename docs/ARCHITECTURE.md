# BOSON System Architecture Layout

## System Modules & Interfaces

1. Application Layer (UCI Interface)
   - Abstract commands from the frontend, translation maps, and standard system streams.
   - Zero visibility into search heuristics or bitboard definitions.

2. Search Layer (Calculation Pipeline)
   - Manages calculation trees, iterative deepening splits, and pruning gates.
   - Interacts strictly with the Evaluation Interface; remains completely agnostic to whether evaluation is Hand-Crafted (HCE) or Neural (NNUE/Transformer).

3. Evaluation Layer (State Scoring)
   - Exposes a unified interface returning a scalar value from a specific position view.

4. Board Layer (State Core)
   - High-performance bit manipulation maps, move applications, and history stacks.
   - Modifiable only via deterministic operations (`makeMove`/`undoMove`).

## System Flow & Boundaries
`UCI Layer ──► Search Layer ──► Evaluation Interface ──► Board Layer`
Strict downstream dependency tracking: Higher modules consume lower interfaces. Lower modules never notify or access higher layers directly.