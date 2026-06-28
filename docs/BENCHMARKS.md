# BOSON — Engine Benchmark Ledger

## Baseline Run: Iterative Deepening & PV Integration
* **Date:** June 2026
* **Compiler:** MSVC 19.51 (C++23)
* **Optimization Flag:** `/O2 /Oi`

### Standard Initial Array Test (FEN: Startpos)

| Depth | Score (cp) | Total Nodes (Base + Q) | Time (ms) | NPS | Principal Variation (PV) |
| :---: | :--------: | :--------------------: | :-------: | :-----: | :---------------------- |
| 1     | +50        | 41                     | 0         | 41      | `b1c3`                  |
| 2     | 0          | 142                    | 1         | 142,000 | `b1c3 b8c6`             |
| 3     | +50        | 1,133                  | 3         | 377,666 | `b1c3 b8c6 g1f3`        |
| 4     | 0          | 3,350                  | 7         | 478,571 | `b1c3 b8c6 g1f3 g8f6`   |

## Time Management & Controller Framework Integration
* **Date:** June 2026
* **Allocation Policy:** Base / 20 + Inc / 2

### Clock Budget Allocation Test (2000ms base + 50ms inc)
* **Soft Limit:** 125ms
* **Hard Limit:** 500ms
* **Exit Reason Code:** 2 (SoftTimeLimit)

| Depth | Score (cp) | Total Nodes | Time (ms) | NPS | Principal Variation (PV) |
| :---: | :--------: | :---------: | :-------: | :-----: | :---------------------- |
| 5     | +5         | 21,044      | 10        | 2.10M   | `b1c3 b8c6 g1h3 g8f6 h3f4` |
| 6     | 0          | 63,382      | 42        | 1.50M   | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| 7     | 0          | 347,413     | 124       | 2.80M   | `b1c3 g8f6 g1f3 b8c6`   |

## Aspiration Windows Framework Integration
* **Date:** June 2026
* **Base Window Delta ($\delta$):** 30 cp
* **Activation Target:** Depth $\ge$ 5

### Aspiration Narrowing Test (FEN: Startpos, Depth 6)
* **Window Successes:** 2
* **Window Failures:** 0 (High: 0, Low: 0)

| Depth | Score (cp) | Total Nodes | Time (ms) | NPS | Principal Variation (PV) |
| :---: | :--------: | :---------: | :-------: | :-----: | :---------------------- |
| 5     | +5         | 19,674      | 16        | 1.22M   | `b1c3 b8c6 g1h3 g8f6 h3f4` |
| 6     | 0          | 61,077      | 81        | 754K    | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |

## Null Move Pruning Integration
* **Date:** June 2026
* **Reduction Constant ($R$):** 2
* **Verification Target:** FEN: Startpos, Depth 6

### 🚀 Engine Optimization Progression History

| Milestone Phase | Score | Total Nodes | Time (ms) | NPS | Principal Variation (PV) |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Baseline Core** | `0` | 63,382 | 42 | 1.50M | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Aspiration Windows** | `0` | 61,077 | 81 | 754K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Null Move Pruning** | `0` | **35,725** | 38 | 940K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |

### Null Move Performance Telemetry
* **Null Move Attempts:** 218
* **Null Move Cutoffs:** 120
* **Null Move Failures:** 98
* **Zugzwang Triggers:** 0 (Initial array contains full non-pawn material)

## Late Move Reductions (LMR) Integration
* **Date:** June 2026
* **Formula:** Logarithmic Table Policy ($s\_reductionTable[64][64]$)
* **Target Test FEN:** Startpos, Depth 6

### 🚀 Engine Optimization Progression History

| Milestone Phase | Score | Total Nodes | Time (ms) | NPS | Principal Variation (PV) |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Baseline Core** | `0` | 63,382 | 42 | 1.50M | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Aspiration Windows** | `0` | 61,077 | 81 | 754K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Null Move Pruning** | `0` | 35,725 | 38 | 940K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Late Move Reductions** | `0` | **6,202** | 12 | 516K  | `g1f3 b8c6 b1c3 g8f6 a1b1 a8b8` |

### LMR Telemetry Data
* **LMR Reduction Attempts:** 372
* **Triggered Re-Searches:** 3 ($0.8\%$)
* **Successful PV Overturns:** 0 ($0\%$ efficiency)

## Continuation History (CONTHIST) Integration
* **Date:** June 2026
* **Memory Footprint:** 196 KB flat matrix array
* **Target Test FEN:** Startpos, Depth 6

### 🚀 Engine Optimization Progression History

| Milestone Phase | Score | Total Nodes | Time (ms) | NPS | Principal Variation (PV) |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Baseline Core** | `0` | 63,382 | 42 | 1.50M | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Aspiration Windows** | `0` | 61,077 | 81 | 754K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Null Move Pruning** | `0` | 35,725 | 38 | 940K  | `b1c3 b8c6 g1f3 g8f6 a1b1 a8b8` |
| **Late Move Reductions** | `0` | 6,202  | 12 | 516K  | `g1f3 b8c6 b1c3 g8f6 a1b1 a8b8` |
| **Counter-Move History** | `0` | 6,203  | 6  | 1.03M | `g1f3 b8c6 b1c3 g8f6 a1b1 a8b8` |
| **Continuation History** | `0` | **6,196** | 6  | 1.03M | `g1f3 b8c6 b1c3 g8f6 a1b1 a8b8` |

### Multi-Ply Local Memory Telemetry
* **Continuation Table Hits:** 380
* **Continuation Cutoffs:** 566
* **CMH Table Hits:** 0 (Pre-empted by higher context priority sorting)
* **Table Normalization Events:** 0

