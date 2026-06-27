# BOSON — Static Exchange Evaluation (SEE)

## Purpose
SEE acts as a localized tactical lookahead simulator. It calculates the material balance outcome of a series of exchanges on a single target square without triggering a full minimax search tree recursion.

## Subsystem Architecture
The SEE logic is completely decoupled from search constraints and resides in:
* `engine/include/search/see/SEE.hpp`
* `engine/src/search/see/SEE.cpp`

## Implementation Highlights
* **Non-Destructive Simulation:** Operates purely on a local copy of the position's `occupancy` bitboard map, avoiding expensive `MakeMove` / `UndoMove` calls.
* **Dynamic Ray Casting:** Features an inline ray-caster to safely track background sliding pieces (Rooks, Bishops, Queens) and instantly reveal discovered attacks as blocking pieces are virtually cleared.
* **Ghost Filtering:** Explicitly cross-references raw bitboards with the modified occupancy mask to prevent virtually captured pieces from continuing to attack.