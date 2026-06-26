# BOSON Move Generation Specification

## 1. Architectural Boundaries
The `MoveGenerator` subsystem is completely side-effect free and acts as a read-only analyst of a `Position` instance. It accepts a read-only reference to the state of the universe and populates a high-performance stack-allocated container called `MoveList`.
$$\text{Position (Read-Only)} \longrightarrow \text{MoveGenerator} \longrightarrow \text{MoveList}$$

## 2. Layered Structural Breakdown
To guarantee maintainable code, generation is decoupled into three strict geometrical filters:
1. **Attack Generation:** Pure mathematical lookup of what squares a piece can physically strike, ignoring king exposure.
2. **Pseudo-Legal Generation:** Creating actual `Move` objects for leaping and sliding paths, ensuring landing zones are either empty or occupied by enemy targets.
3. **Legal Filtering:** Transactionally passing a pseudo-legal move to `MoveExecutor::makeMove()`, verifying if the friendly king is under attack, and rejecting the move if it fails safety checks.

## 3. Pre-Calculated Leaper Attack Arrays
Leaping pieces (Knights and Kings) travel along constant geometric relative offsets. Rather than calculating these offsets during a lookahead search, Boson computes an array of 64 pre-calculated bitboards at boot time.