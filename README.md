# Boson

Boson is a chess engine written in C++23, designed as a research platform for exploring search algorithms, bitboard manipulation, and evaluation architectures. It implements the standard Universal Chess Interface (UCI) protocol.

The engine prioritizes deterministic state management, clean module boundaries, and measurable benchmark verification over premature micro-optimization.

---

## Technical Overview

- **Board Representation:** 64-bit bitboards for piece placement and composite occupancies. Ray-based sliding piece attacks, precalculated leaper masks (knights, kings), and pawn structure logic with BMI2 hardware instruction awareness (`_pext_u64`, `_tzcnt_u64`, `_blsr_u64`).
- **State Transitions:** Value-semantic `Position` representation with transactional `makeMove` and `undoMove` mechanics using `UndoState` records. State is stack-allocated by default to prevent cross-thread synchronization issues.
- **Move Generation:** Strictly legal move generation pipeline with fast pin and check detection (`isSquareAttacked`, `inCheck`).
- **Search Engine:** Alpha-Beta Negamax with iterative deepening, aspiration windows, and principal variation (PV) extraction.
- **Selective Search & Pruning:** Quiescence search with stand-pat delta cutoffs, Static Exchange Evaluation (SEE), Null Move Pruning (NMP) with zugzwang guards, and logarithmic Late Move Reductions (LMR) with null-window scout re-searches.
- **Move Ordering:** Transposition table hash move, MVV-LVA capture sorting, Killer moves (2-ply slots), Counter-Move History (CMH), 2-Ply Continuation History, and Global History tables.
- **Evaluation:** Hand-Crafted Evaluation (HCE) with tapered piece-square tables and material values, complemented by a dynamic Correction History table that tracks and compensates for evaluator bias.
- **Protocol:** Standard UCI protocol loop supporting standard time controls (`wtime`, `btime`, `winc`, `binc`, `movetime`, `depth`, `infinite`).

---

## Architecture and Subsystems

Boson isolates subsystems behind unidirectional interfaces. Higher layers consume lower interfaces; lower layers maintain no dependencies on higher modules.

```
UCI Protocol (Application)
        │
        ▼
Search Controller & Negamax Engine
  ├── Iterative Deepening & Aspiration Windows
  ├── MoveOrderer (TT, MVV-LVA, SEE, Killers, CMH, Continuation, History)
  ├── Transposition Table (64-bit Zobrist Hash)
  └── TimeManager
        │
        ▼
Evaluator (State Scoring Interface)
  ├── Hand-Crafted Evaluation (PST + Material)
  └── Correction History Table
        │
        ▼
Board Core (State Representation)
  ├── Position (12 piece bitboards + 3 occupancy bitboards)
  ├── MoveGenerator (Ray tables, leaper masks, legality filtering)
  ├── MoveExecutor & UndoState
  └── FenParser & BoardPrinter
```

Architectural specifications and subsystem contracts are documented in `docs/ARCHITECTURE.md`.

---

## Repository Structure

```
Boson/
├── engine/                 # Core engine implementation
│   ├── CMakeLists.txt      # Build configuration for engine and test targets
│   ├── include/            # Header files organized by subsystem
│   │   ├── board/          # Bitboard, Position, Move, MoveExecutor, MoveGenerator
│   │   ├── debug/          # BoardPrinter and diagnostic utilities
│   │   ├── evaluation/     # Evaluator, PieceSquareTables, CorrectionHistoryTable
│   │   ├── fen/            # FenParser and board state loader
│   │   ├── search/         # Search, TranspositionTable, MoveOrderer, SEE, history tables
│   │   └── validation/     # Verification harness and invariants
│   └── src/                # Translation units corresponding to include/
├── tests/                  # Verification suite and tactical datasets
│   ├── TestRunner.cpp      # Test suite entry point
│   └── tactical/           # Perft reference files and Win-At-Chess (WAC) EPDs
├── docs/                   # Architectural charters, specifications, and telemetry logs
│   ├── VISION.md           # Research platform scope and design goals
│   ├── ARCHITECTURE.md     # Module boundaries, data flow, and replaceability contracts
│   ├── ROADMAP.md          # Milestones 0 through 10 and Milestone Omega
│   ├── PRINCIPLES.md       # The 5 immutable engineering principles
│   ├── CODING_STANDARD.md  # C++23 standards, naming, and memory conventions
│   ├── BENCHMARKS.md       # Performance telemetry and regression records
│   └── ...                 # Algorithm-specific design docs
├── scripts/                # Build and continuous integration automation
├── tools/                  # Tuning harnesses, opening book tools, match runners
├── CHANGELOG.md            # Milestone version history
├── LICENSE                 # MIT License
└── README.md               # Repository entry point
```

---

## Build Requirements

- **Language Standard:** C++23 (`set(CMAKE_CXX_STANDARD 23)`)
- **Build System:** CMake 3.25 or newer
- **Compiler Compatibility:**
  - Microsoft Visual C++ 2022 (v143+) or newer (`/W4 /WX /Oi /O2 /permissive-`)
  - GCC 13+ (`-Wall -Wextra -Wpedantic -Werror -O3 -mavx2 -mbmi2 -mpopcnt`)
  - Clang 16+ (`-Wall -Wextra -Wpedantic -Werror -O3 -mavx2 -mbmi2 -mpopcnt`)

All builds enforce zero compiler warnings. Warnings are treated as fatal errors across all configurations.

---

## Building and Verification

### 1. Build the Test Suite

```bash
# Configure build directory targeting engine/
cmake -S engine -B build -DCMAKE_BUILD_TYPE=Release

# Compile test runner
cmake --build ./build --config Release --target boson_tests
```

### 2. Run Test Suite

```bash
./build/bin/Release/boson_tests.exe
```

The test runner validates:
- Move generation accuracy and perft node counts.
- State rollback symmetry across makeMove/undoMove cycles.
- Search heuristics telemetry (Aspiration Windows, NMP, LMR, CMH, Continuation History, Correction History).
- Tactical benchmark solving across standard test positions.

### 3. Build the Engine Executable

```bash
cmake --build ./build --config Release --target boson
```

The engine executable is output to `build/bin/Release/boson.exe` (or `boson` on Linux/macOS).

---

## Core Engineering Principles

1. **Correctness before Speed:** Move legality, board state consistency, and search invariance take precedence over raw node throughput.
2. **Architecture before Optimization:** Code must adhere to strict subsystem boundaries before profiling and hot-path tuning.
3. **Measure before Changing:** Intuition is disregarded. All algorithmic additions require fixed-depth benchmark telemetry and SPRT match data.
4. **Replaceable Modules:** Major components (Evaluation, Search, Board) are isolated behind interfaces to allow drop-in replacement (e.g., swapping HCE for NNUE).
5. **Every Commit Improves Boson:** Zero compiler warnings, 100% test pass rate, and full documentation updates are required on every change.

Detailed explanations are maintained in `docs/PRINCIPLES.md`.

---

## License

Boson is licensed under the [MIT License](LICENSE).
