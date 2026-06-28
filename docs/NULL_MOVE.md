# BOSON — Null Move Pruning (NMP)

## Principle
Null Move Pruning optimizes search speed by checking if a player can skip their turn entirely and still maintain a score above beta. If skipping a turn still results in a fail-high, the branch is strong enough to trigger an immediate cutoff.

## Architecture & Safety Guardrails
* **Isolated to Alpha-Beta:** Pruning decisions are kept strictly inside the search loop to keep external subsystems clean.
* **Anti-Recursive Safety:** The `allowNull` flag prevents back-to-back null searches.
* **Zugzwang Avoidance:** NMP automatically turns off if the active side is in check or if they have no remaining non-pawn material.
* **Transposition Table Support:** Cutoffs are stored as lower bounds in the TT to preserve search accuracy.