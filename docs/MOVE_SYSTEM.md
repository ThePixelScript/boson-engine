# BOSON Move & State Transition Specification

## 1. The Transactional Loop Invariant
To maximize cache performance and search depth, Boson avoids cloning `Position` states during node traversals. The engine relies on a strict mathematical invariant:
$$Position_{t} \xrightarrow{\text{MakeMove(M)}} Position_{t+1} \xrightarrow{\text{UndoMove(M, State)}} Position_{t}$$
The state of the universe after an execution followed by its inversion must be bit-for-bit identical to its initialization.

## 2. Minimal State Tracking (UndoState)
Rather than snapshotting the entire board, `UndoState` strictly tracks irreversible information or state variables overwritten during a move:
- Overwritten Castling Rights
- Overwritten En Passant Target Squares
- Overwritten Halfmove Clock values
- Captured Piece Identity (to restore bits popped out during captures)

## 3. Strict Deterministic Mutation Order
To ensure predictable execution and prevent ghost piece errors, every state modification follows an unvarying algorithmic timeline:
1. Copy current game flags into `UndoState`.
2. Clear the moving piece's bit from its source square.
3. If a capture occurs, remove the victim piece's bit from the target square.
4. Set the moving piece's bit on the destination square.
5. Force an incremental rebuild of the combined occupancy masks.
6. Advance rule clocks, resolve promotions/special flags, and toggle active side.