# BOSON Technical Roadmap

## Milestone 0: Platform Genesis [CURRENT]
- Setup CMake targets with strict compiler diagnostic requirements.
- Initialize UCI interface echo loop and state engine skeleton.

## Milestone 1: State Processing & FEN
- Implement hardware-native Bitboard structures.
- Construct legal FEN parser and deterministic board output formatter.

## Milestone 2: Move Generation & Validation
- Implement sliding piece attack structures via hardware BMI2 instructions.
- Create automated Perft framework passing 100% of standard benchmark positions.