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