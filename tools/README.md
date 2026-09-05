# Boson Tools Directory

This directory is reserved for developer utilities, offline optimization tools, and evaluation tuning frameworks for the Boson Chess Engine.

## Scope & Purpose

- **Evaluation Parameter Tuning**: Offline tuners (e.g., Texel's Tuning Method implementation) to optimize Piece-Square Tables (PST) and positional evaluation terms against annotated grandmaster and self-play game datasets.
- **Opening Book Tools**: Polyglot opening book converters, book probing harnesses, and opening tree generators.
- **Dataset Generators & EPD Tools**: Tools for extracting, filtering, and normalizing EPD/PGN datasets, perft test suites, and tactical test positions.
- **Tournament Orchestration**: Configuration files and wrapper runners for automated engine-versus-engine match frameworks (e.g., `cutechess-cli`, `fastchess`) executing Sequential Probability Ratio Tests (SPRT).
- **Profile-Guided Optimization (PGO)**: Tooling to capture execution profiles and drive PGO compile passes.
