# BOSON — Correction History (CORRHIST)

## Philosophy
Correction History does not assist move ordering. Instead, it serves as a dynamic error-correction layer for the Evaluation Subsystem. It tracks position features to learn structural evaluation bias: *Whenever my static evaluation algorithms see positions like this, do they tend to over or underestimate reality?*

## Pipeline Structure
Material Assessment ➔ PST Adjustments ➔ Specialized Evaluator Terms ➔ **Correction History Interpolation** ➔ Final Position Score

## Architectural Boundaries
* **Ownership:** `Evaluation` Subsystem (Searches lookup entries in an entirely read-only, immutable state).
* **Update Vector:** `Search` Engine (Updates bias weights exclusively on high-reliability Exact node evaluation completions with depth $\ge 4$).
* **Future Compatibility:** Operates as a permanent top-layer calibration filter, completely uncoupled from whether the underlying evaluation score is generated via classical evaluation or a future NNUE engine backend.