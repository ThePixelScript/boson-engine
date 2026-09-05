# Static Exchange Evaluation (SEE) Architecture & Invariants

## 1. Architectural Purpose & Theoretical Foundation

Static Exchange Evaluation (SEE) is an algorithmic technique for evaluating tactical balance on a single target square without expanding recursive game-tree nodes. In complex tactical positions, multiple pieces of varying values converge on a disputed square (e.g., $d5$ or $e4$). 

Instead of executing full `makeMove` / `undoMove` transitions or invoking positional evaluation functions, SEE isolates the exchange sequence:
- It sequences virtual captures on the target square from the Least Valuable Attacker (LVA) to the most valuable.
- It models captures by alternately updating a local, simulated occupancy bitboard mask.
- It resolves the exchange sequence via minimax backward induction, allowing either side to stand pat (terminate the exchange) if further recaptures would incur a net material deficit.

This operation computes the exact static capture payoff in $O(k)$ operations (where $k \le 32$ is the exchange depth, typically $k \le 6$).

---

## 2. Architectural Isolation: Tactical Search vs. Positional Evaluation

Boson strictly separates static exchange evaluation from positional evaluation:
- **Positional Evaluator ([`Evaluator`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/include/evaluation/Evaluator.hpp))**:
  Computes whole-board positional attributes: material balance, piece-square table bonuses (PSQT), pawn structure, king safety, and mobility. Operates as a static leaf heuristic.
- **Static Exchange Evaluator ([`SEE`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/include/search/see/SEE.hpp))**:
  Lives strictly under `search/see/` because it is a **search-tactical pruning and ordering oracle**. It does not consider positional nuance, positional compensation, or whole-board pawn structure; it evaluates only immediate material turnover under forced tactical liquidation.

### Zero-Mutation Invariant
SEE operates strictly on `const Position& pos`. It:
1. **Never** calls `MoveExecutor::makeMove()` or `MoveExecutor::undoMove()`.
2. **Never** mutates the piece arrays, mailbox, king squares, castling rights, or Zobrist hash key.
3. Maintains all virtual board state transformations purely inside a local 64-bit integer (`Bitboard occupancy`).

---

## 3. Algorithm Specification & Occupancy Modeling

### 3.1 Initial State Setup
Given a move from square $S_{\text{from}}$ to target square $S_{\text{to}}$ in position $P$:
1. Determine `attacker = pieceAt(pos, fromSq)` and `victim = pieceAt(pos, toSq)`.
2. Handle en-passant: if $S_{\text{to}} = \text{pos.getEnPassantSquare()}$ and `attacker` is a pawn, `victim` is set to the enemy pawn.
3. Initialize the exchange gain tracking array:
   $$\text{gain}[0] = \text{value}(\text{victim})$$
4. Form the local simulated occupancy mask:
   $$\text{occupancy} = \text{pos.getTotalOccupancy()} \setminus \{S_{\text{from}}\}$$

### 3.2 Dynamic Attacker Identification & Discovered X-Rays
To identify which pieces attack the target square $S_{\text{to}}$ under the simulated `occupancy`:
- **Pawns**: Reverse-geometry masks test diagonal capture origins.
- **Knights**: Static precomputed knight attack masks test potential knight origins.
- **Kings**: One-step king neighborhood masks test king proximity.
- **Sliding Pieces (Bishops, Rooks, Queens)**:
  Ray casting radiates outward from $S_{\text{to}}$ in the 4 orthogonal and 4 diagonal directions. The ray advances until it intersects the first set bit in `occupancy`:
  - If that bit belongs to an enemy or friendly slider with the matching ray capability, it is added to `attackers`.
  - The ray then halts.

**Dynamic X-Ray Discovery**:
When an attacker on square $S_{\text{att}}$ captures on $S_{\text{to}}$, its source square is removed from the occupancy mask:
$$\text{occupancy} \leftarrow \text{occupancy} \setminus \{S_{\text{att}}\}$$
Calling `getAttackers()` with the updated `occupancy` mask allows subsequent ray scans to pass through the vacated $S_{\text{att}}$ square. Any slider positioned behind $S_{\text{att}}$ along that same file, rank, or diagonal (e.g., a rook behind a queen on an open file) is dynamically discovered as a new attacker.

### 3.3 Least Valuable Attacker (LVA) Sequencing
At each exchange step $d$, the side to move $C$ queries `getLeastValuableAttacker()`:
1. Iterates in ascending material order:
   $$\text{Pawn} \rightarrow \text{Knight} \rightarrow \text{Bishop} \rightarrow \text{Rook} \rightarrow \text{Queen} \rightarrow \text{King}$$
2. Selects the lowest-value piece belonging to color $C$ whose bit is present in `attackers & pos.getPieceBitboard(p)`.
3. If no attacker exists for side $C$, the exchange sequence terminates.
4. Otherwise, compute the step gain delta:
   $$\text{gain}[d] = \text{value}(\text{attacker}) - \text{gain}[d - 1]$$
5. Update active attacker to the selected piece, clear its square from `occupancy`, and loop.

### 3.4 Minimax Backward Induction
Once all attackers are exhausted or exchange depth limit (32) is reached, the optimal exchange score is resolved via negamax backward induction:
$$\text{gain}[d - 1] = -\max(-\text{gain}[d - 1], \text{gain}[d])$$
decrementing $d$ to 0.

This formulation accounts for voluntary capture decisions: at any ply in the liquidation chain, a side can refuse to capture if doing so yields a worse outcome than standing pat.

---

## 4. Integration Boundaries

### 4.1 Move Ordering ([`MoveOrderer`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/src/search/MoveOrderer.cpp))
Captures generated during search are partitioned into distinct priority tiers based on `SEE::evaluate()`:
- **Tier 2 (Good & Equal Captures)**: $\text{SEE}(m) \ge 0$.
  Ordered via MVV-LVA bonuses ($8\,000\,000 + \text{MVV\_LVA}$).
- **Tier 9 (Losing Captures)**: $\text{SEE}(m) < 0$.
  Severely penalized ($-8\,000\,000 + \text{seeValue}$), deferring catastrophic blunders (e.g. $Q \times P$ defended by pawn) until after all quiet moves, killers, countermoves, and history heuristics.

### 4.2 Quiescence Search ([`Search::quiescence`](file:///C:/Users/Dell/Desktop/Programming/Project/Boson/engine/src/search/Search.cpp#L85-L115))
In quiescence search:
- When not in check and the move is not a pawn promotion:
  $$\text{if } (\text{SEE::evaluate}(\text{pos}, m) < 0) \implies \text{prune capture}$$
- Pruning losing captures eliminates the horizon effect without wasting tree expansion on suicidal captures that the opponent can trivially refute.