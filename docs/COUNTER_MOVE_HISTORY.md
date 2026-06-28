# BOSON — Counter-Move History (CMH)

## Purpose
Counter-Move History acts as a contextual move-ordering heuristic. While standard history tables record global move success across the entire board state, CMH establishes an immediate associative tracking bond: *Given the previous move, what specific reply historically forces a beta cutoff?*

## Architectural Ownership & Memory Footprint
* **Owner:** `Search` Subsystem (Writes refutations instantly upon a verified beta cutoff).
* **Consumer:** `MoveOrderer` Subsystem (Queries the live matrix to elevate contextual replies).
* **Memory Structure:** Dense 2D array mapped as `std::array<std::array<Move, 64>, 64>`. 
* **Cache Locality:** Measures exactly 16 KB, fitting entirely inside the CPU's hardware L1 Data Cache for $O(1)$ fast lookups.

## Priority Sorting Order
1. Transposition Table (TT) Move
2. Winning Captures (SEE > 0) / Equal Captures (MVV-LVA)
3. Pawn Promotions
4. Killer Move 1
5. Killer Move 2
6. **Counter-Move History (CMH) Match** <-- *Contextual Injection Point*
7. Global History Table Score
8. Quiet Moves
9. Losing Captures (SEE < 0)