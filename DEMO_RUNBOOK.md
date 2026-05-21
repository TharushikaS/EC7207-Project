
---

## Pre-demo setup 

```bash
cd ~/EC7207-Project
clear
```

- Close OneDrive sync, Chrome tabs, anything else eating CPU.
- Plug in the laptop (battery saver throttles cores).
- In a second terminal, pre-launch the dashboard so the browser tab is warm:
  ```bash
  cd ~/EC7207-Project
  source venv/bin/activate
  streamlit run src/dashboard/app.py
  ```

---

## Phase 1 — Intro (2 min)

- The problem: simulate tsunami wave propagation.
- The model: **2D linear wave equation** `∂²h/∂t² = c²∇²h`, a small-amplitude / constant-depth linearization of the Shallow Water Equations (Pedlosky 1987 §3.9).
- Discretization: **leap-frog in time + 5-point Laplacian in space** (Strikwerda 2004 Ch. 12; Courant–Friedrichs–Lewy 1928).
- Four implementations: Serial, OpenMP, MPI, Hybrid.
- Open the four `.cpp` files in VS Code side-by-side.

---

## Phase 2 — Live compilation (1 min)

> *"Same equation, four compile commands."*

```bash
cd src/serial && g++ -O3 -o tsunami_serial tsunami_serial.cpp && cd ../..
cd src/openmp && g++ -O3 -fopenmp -o tsunami_omp tsunami_omp.cpp && cd ../..
cd src/mpi    && mpic++ -O3 -o tsunami_mpi tsunami_mpi.cpp && cd ../..
cd src/hybrid && mpic++ -O3 -fopenmp -o tsunami_hybrid tsunami_hybrid.cpp && cd ../..
```

**Narration:**
- Serial: plain `g++`
- OpenMP: adds `-fopenmp` flag
- MPI: uses `mpic++` wrapper (g++ + MPI headers/libs)
- Hybrid: both

If asked *"what does -fopenmp do?"*: *"Tells the compiler to recognize OpenMP pragmas and link the OpenMP runtime."*

---

## Phase 3 — Live execution (1 min)

> *"Watch the execution times."*

```bash
cd src/serial && ./tsunami_serial && cd ../..
cd src/openmp && ./tsunami_omp 1 && ./tsunami_omp 4 && cd ../..
cd src/mpi    && mpiexec -n 4 ./tsunami_mpi && cd ../..
cd src/hybrid && mpiexec -n 2 ./tsunami_hybrid 4 && cd ../..
```

**Narration during runs:**
- Serial: *"~5 seconds baseline."*
- OpenMP 1t: *"Same code through OpenMP framework — slightly slower because of OpenMP setup cost with no parallelism."*
- OpenMP 4t: *"Now 4 threads — about 3× faster."*
- MPI 4p: *"MPI splits the grid into 4 horizontal slices that exchange halo rows. Different mechanism, comparable speedup."*
- Hybrid 2×4: *"MPI between processes + OpenMP within each process — two-level parallelism, 8 cores total."*

---

## Phase 4 — Dashboard walk-through (5 min)

Switch to browser. The dashboard reads `data/benchmarks.csv` (pre-computed scaling study) and `data/ground_truth/*.bin` (frames from the actual runs).

- Pick **Serial** → show wave animation. *"This is the ground truth."*
- Pick **OpenMP** → same wave. *"Bit-identical to serial — proves parallel correctness."*
- Pick **MPI**, then **Hybrid** → same wave each time.
- Show timing panel on the right — execution time, speedup, efficiency, cores used.

---

## Phase 5 — Discussion of results (3 min)

**Key talking points (use as many as fit):**

1. **OpenMP-1 slower than Serial.** Expected — OpenMP runtime initialization + barrier synchronization have non-zero cost. With no parallelism to amortize them, you pay overhead with no benefit.

2. **Performance plateaus past 4 cores.** This machine has 4 physical + 4 hyperthreads. Hyperthreads share floating-point units; our stencil is FP-heavy. Hyperthreading helps integer workloads, hurts FP-bound stencil codes. Well-known HPC result.

3. **OpenMP efficiency drops 73% → 42%** (2 threads → 4 threads). Memory-bandwidth ceiling. Once 2 threads saturate the memory bus, extra threads wait. Classic stencil-code behavior — Hager & Wellein 2010, Ch. 5.

4. **MPI-2 hits 95% efficiency** — better than OpenMP-2. Separate memory spaces, no false sharing, no cache-coherence traffic.

5. **Run-to-run variance ~10–20%.** WSL doesn't pin processes; background processes share CPU; cache warmup varies. Production HPC uses 3-run medians with `taskset` pinning. Noted as future work.

**Backup phrase if challenged on the small speedup:**
> *"Our peak measured speedup is 1.09× for MPI-2 processes. That looks small, but it's diagnostic, not a failure. A single thread already saturates this CPU's memory bandwidth at our problem size — roughly 80 GB of memory traffic at ~20 GB/s gives ~4 seconds, which matches every configuration we measured. The Roofline model (Williams, Waterman, Patterson 2009; Hager & Wellein 2010) predicts exactly this ceiling. Breaking through it requires cache-blocking — increasing arithmetic intensity by computing multiple timesteps per data load. We discuss this in the report's future-work section."*

---

## Phase 6 — Q&A (3 min)

---

## Failure modes and recovery

| Symptom | Fix |
|---------|-----|
| `mpiexec: not enough slots` | Add `--oversubscribe`: `mpiexec --oversubscribe -n 8 ./tsunami_mpi` |
| Run takes 30s instead of 5s | Background load. Say *"some system load — let me re-run"* and retry. |
| `mpic++: command not found` | `sudo apt install openmpi-bin libopenmpi-dev` (won't happen if pre-tested) |
| Dashboard shows mock-data banner | Click **Refresh data** in sidebar |

If asked *"show me 8 threads"*:
```bash
cd src/openmp && ./tsunami_omp 8 && cd ../..
```
Then explain hyperthreading diminishing returns.

---

## Cheat sheet (one-liners)

```bash
# COMPILE
cd src/serial && g++ -O3 -o tsunami_serial tsunami_serial.cpp && cd ../..
cd src/openmp && g++ -O3 -fopenmp -o tsunami_omp tsunami_omp.cpp && cd ../..
cd src/mpi    && mpic++ -O3 -o tsunami_mpi tsunami_mpi.cpp && cd ../..
cd src/hybrid && mpic++ -O3 -fopenmp -o tsunami_hybrid tsunami_hybrid.cpp && cd ../..

# RUN
cd src/serial && ./tsunami_serial && cd ../..
cd src/openmp && ./tsunami_omp 4 && cd ../..
cd src/mpi    && mpiexec -n 4 ./tsunami_mpi && cd ../..
cd src/hybrid && mpiexec -n 2 ./tsunami_hybrid 4 && cd ../..
```
