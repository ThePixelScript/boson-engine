# C++23 Coding Standard

This document specifies the technical standards, naming conventions, and design patterns required for the Boson codebase.

---

## 1. Compiler and Language Standard

- **Standard:** ISO C++23 (`set(CMAKE_CXX_STANDARD 23)` required, extensions disabled).
- **Diagnostics Policy:** Zero compiler warnings allowed. Warnings are escalated to errors.
  - **MSVC:** `/W4 /WX /Oi /O2 /permissive-`
  - **GCC / Clang:** `-Wall -Wextra -Wpedantic -Werror -O3 -mavx2 -mbmi2 -mpopcnt`
- **Portability:** Code must compile cleanly across MSVC, GCC, and Clang on x86-64 hardware. Target-specific intrinsics (such as BMI2 or AVX2) must be guarded or wrapped behind compiler-agnostic bit manipulation primitives.

---

## 2. Naming Conventions

Consistency across identifiers simplifies navigation and prevents ambiguity in search and board routines.

| Category | Convention | Examples |
| :--- | :--- | :--- |
| **Types, Classes, Structs, Enums** | `PascalCase` | `Position`, `MoveExecutor`, `TranspositionTable`, `Square`, `Color` |
| **Methods and Functions** | `camelCase` | `makeMove()`, `isSquareAttacked()`, `runSearch()`, `quiescence()` |
| **Local Variables & Parameters** | `camelCase` | `sideToMove`, `alpha`, `beta`, `bestMove`, `fromSquare` |
| **Member Variables (Private/Protected)** | `m_camelCase` | `m_pieces`, `m_occupancy`, `m_sideToMove`, `m_hashKey` |
| **Plain Struct Fields (POD/DTO)** | `camelCase` | `moves`, `count`, `wtime`, `depth`, `alpha` |
| **Static Class Members** | `s_camelCase` | `s_tt`, `s_cmTable`, `s_chTable`, `s_killerMoves` |
| **Constants and Enumerators** | `UPPER_SNAKE` | `INF`, `MATE`, `NODE_CHECK_PERIOD`, `MAX_PLY` |
| **Namespaces** | `PascalCase` or `lowercase` | `Boson` (root), `Boson::Search` |

---

## 3. Strong Typing Over Primitive Types

Raw integers must not be used where a domain type exists. Strong typing catches logic errors at compile time and prevents unit/domain mixing.

- **Chess Primitives:** Always use `Square`, `Color`, `Piece`, and `Move` instead of raw integers.
  ```cpp
  // Correct
  void setPiece(Square sq, Piece piece) noexcept;
  
  // Incorrect: raw ints obscure meaning and invite parameter swapping
  void setPiece(int sq, int piece) noexcept;
  ```
- **Bitboards:** Represented explicitly as `Bitboard` (`uint64_t`). Do not alias or mix with general 64-bit integers unless performing bitwise arithmetic.
- **Fixed-Width Integers:** Use `<cstdint>` types (`uint8_t`, `int16_t`, `uint32_t`, `uint64_t`) rather than platform-variable types (`long`, `unsigned int`).
- **Scoped Enums:** All enumerations must be declared as `enum class` with explicit underlying storage types:
  ```cpp
  enum class Color : uint8_t {
      White = 0,
      Black = 1,
      None  = 2
  };
  ```

---

## 4. Modern C++23 Idioms

- **Bit Operations:** Use standard `<bit>` facilities (`std::popcount`, `std::countl_zero`, `std::countr_zero`) or dedicated intrinsics instead of custom bit-loop hacks.
- **Attributes:**
  - Use `[[nodiscard]]` on functions returning values that should not be ignored (e.g., query methods, evaluate results, state checks).
  - Use `noexcept` on all functions that are guaranteed not to throw, especially board mutations, bit manipulation, and search hot paths.
- **Const Correctness:** Member functions that do not mutate state must be marked `const noexcept`. Parameters passed by reference that are not modified must be `const&`.
- **Structured Error Handling:** Avoid throwing exceptions in performance-critical code (search loops, move generation). Use return codes, `std::optional`, or `std::expected` for operations that can fail gracefully (e.g., FEN parsing).

---

## 5. Memory Management and Performance

- **Zero Naked Heap Allocations:** Dynamic allocation (`new`, `malloc`) is forbidden in the search hot path. State structures like `Position` and `MoveList` must be stack-allocated.
- **Value Semantics:** Classes like `Position` and `Move` rely on pure value semantics. They must be trivially copyable or efficiently movable.
- **Cache-Line Alignment:** Performance-critical lookup tables and search history structures should be aligned to cache lines where appropriate:
  ```cpp
  struct alignas(64) TTCluster {
      // 64-byte aligned transposition table bucket
  };
  ```
- **Inner-Loop Efficiency:** In `negamax` and `quiescence`:
  - Avoid branching on static invariants.
  - Avoid function call overhead for trivial operations; use inline methods.
  - Avoid verbose logging or I/O within search tree nodes. Output is restricted to root updates and UCI `info` intervals.
