# BOSON Board Architecture Specification

## 1. Core Intent of the Position Abstraction
The `Position` class is the absolute state of the universe within Boson. Every external engine subsystem (Search, Evaluation, UCI, or Move Generation) acts as a consumer of this state. Subsystems receive a read-only view of a `Position` instance and extract scalar data or evaluate its structure without modifying its data invariants.

## 2. Memory Ownership and Lifecycle
- **Ownership:** `Position` completely owns all memory wrappers representing the active state of a game. This includes piece bitboards, combined occupancy masks, side-to-move toggles, castling registries, and en-passant tracking variables.
- **Value Semantics:** To prevent state leaks and memory synchronization errors across asynchronous searching threads, `Position` features value semantics. It is fully copyable and stack-allocated by default.
- **State Mutability Boundaries:** During search, a `Position` is strictly immutable. Transitions to a new state are handled deterministically through copy operations or strict transactional interfaces (`makeMove` / `undoMove`).

## 3. Data Invariant Structure (No Arbitrary Duplication)
To avoid out-of-sync tracking bugs, state values are never duplicated without a strict mathematical connection. Boson tracks:
- **12 Primary Bitboards:** Independent 64-bit masks for each piece-color combination.
- **3 Combined Occupancy Bitboards:** Composite masks (White, Black, and Total) updated incrementally alongside the primary bitboards.

## 4. Future Extension Strategy
The `Position` layout exposes clean structural lookups. When optimization modules (such as BMI2/PEXT sliding lookups or NNUE neural accumulator updates) are introduced in later milestones, they will be bound to the low-level bitboard modification steps without changing the public signatures of the `Position` class.