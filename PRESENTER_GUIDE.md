# Presenter's Guide — EC7207 Group 48

Complete walkthrough: presentation narration, live demo execution, code deep-dive, and Q&A prep.

---

## 0. PRE-FLIGHT (15 minutes before the evaluation)

**On your laptop:**

1. Plug in the charger (battery saver throttles cores → bad timing variance).
2. Close OneDrive sync (right-click tray icon → Pause syncing → 8 hours).
3. Close Chrome tabs except 1 for dashboard, 1 for slides.
4. Close Spotify, Discord, anything else using CPU.

**Open three windows side-by-side:**
- WSL terminal #1 (for compile + run commands) — `cd ~/EC7207-Project`
- WSL terminal #2 (for streamlit) — already running: `streamlit run src/dashboard/app.py`
- VS Code with the four `.cpp` files open in split view

**Sanity-check before sir arrives:**
```bash
cd ~/EC7207-Project
ls src/serial/tsunami_serial   # binary should exist
./src/serial/tsunami_serial    # should finish in ~5s
```
If serial works, all four work. If not — recompile (see Part 3).

**Browser tabs ready:**
- Dashboard at `http://localhost:8501`
- PowerPoint slides open in Slideshow mode

---

## 1. PRESENTATION — slide-by-slide narration

### Slide 1 — Title (10 sec)
> *"Good morning Sir. We are Group 48 — Surasinghe, Tamasha, and Tharshihan. Our project is **High-Performance Simulation of Tsunami Wave Propagation using Parallel Computing Models.**"*

**Click → Slide 2.**

---

### Slide 2 — The Problem (45 sec)
> *"Accurate tsunami simulation is critical for disaster mitigation, but it's computationally expensive. At high resolution, we update one million grid cells every timestep, and we run thousands of timesteps. Serial execution doesn't finish in useful time."*

> *"Our project takes that workload and decomposes it across multiple cores using three parallel paradigms — Shared Memory, Distributed Memory, and Hybrid — then benchmarks each against a serial baseline to measure speedup."*

**Click → Slide 3.**

---

### Slide 3 — Mathematical Model (75 sec)

> *"The simulation is governed by the **2D linear wave equation**:"*

(point to the formula)

> *"The second time-derivative of water height equals the wave speed squared times the spatial Laplacian."*

> *"Here, **h(x,y,t)** is the water surface elevation, and **c** is the wave celerity — for tsunamis, **c equals the square root of gravity times mean ocean depth**."*

> *"This equation is the small-amplitude, constant-depth **linearization of the Shallow Water Equations** — the standard model for the **deep-ocean propagation phase** of a tsunami, before the wave reaches the continental shelf. We are not modelling runup or breaking — that requires the full nonlinear Shallow Water Equations."*

> *"The model is well-established. We cite Pedlosky's Geophysical Fluid Dynamics for the derivation, LeVeque's Finite Volume Methods textbook, Titov and Synolakis's 1998 paper on the MOST tsunami model, and Whitham's classical Linear and Nonlinear Waves."*

**Click → Slide 4.**

---

### Slide 4 — Numerical Scheme (45 sec)

> *"We discretize this PDE using **second-order central differences in both time and space** — known as the **leap-frog scheme** with a **5-point Laplacian stencil**."*

(point to the formula box)

> *"At each interior cell, we compute the next timestep value as: twice the current minus the previous, plus the Courant-number-squared times the sum of the four neighbours minus four times the centre."*

> *"This gives **second-order accuracy** in both space and time — that's O of delta-t-squared plus delta-x-squared."*

> *"Stability is governed by the **Courant–Friedrichs–Lewy condition from 1928**: the Courant number c-times-delta-t-over-delta-x must be less than or equal to one-over-root-two, approximately 0.707. Our setup uses Courant equal to 0.5 — well within the stable region."*

> *"The scheme is foundational — referenced in Strikwerda's Finite Difference Schemes, LeVeque's textbook, and Numerical Recipes."*

**Click → Slide 5.**

---

### Slide 5 — Four Implementations (45 sec)

> *"We implemented **four versions** of the same solver, each using a different parallelization strategy."*

(point to each card)

> *"**Serial** — single-threaded C++ baseline. Standard nested loop. This is our ground truth for accuracy."*

> *"**OpenMP** — shared memory threading. We add a `#pragma omp parallel for collapse(2)` directive over the y and x loops, and OpenMP automatically distributes iterations across CPU threads."*

> *"**MPI** — distributed memory. The grid is split into horizontal slices, one per process. Before each timestep, neighbouring processes exchange their boundary rows using non-blocking `MPI_Isend` and `MPI_Irecv`."*

> *"**Hybrid** — combines both. MPI distributes slices across processes; OpenMP threads parallelize within each process. Two-level parallelism."*

**Click → Slide 6.**

---

### Slide 6 — Build & Execution (30 sec, this is the bridge to the demo)

> *"Each implementation has a slightly different compile command. **Serial** uses plain `g++`. **OpenMP** adds the `-fopenmp` flag. **MPI** uses the `mpic++` wrapper — that's just `g++` with the MPI library headers pre-linked. **Hybrid** combines both."*

> *"Running them differs too. Serial and OpenMP are direct executables. MPI and Hybrid go through `mpiexec`, which launches the requested number of processes."*

> *"Our benchmark configuration: **1000-by-1000 grid, 2000 timesteps, on a 4-physical-core x86 CPU under WSL2.**"*

> *"With your permission Sir, let me show this running live."*

**Click → Slide 7 (LIVE DEMO). Switch to the WSL terminal.**

---

## 2. LIVE DEMO — terminal walkthrough

### Phase A — Compile (40 sec)

**Switch to WSL terminal #1.** Paste these one at a time, narrating each.

```bash
cd src/serial && g++ -O3 -o tsunami_serial tsunami_serial.cpp && cd ../..
```
> *"Plain g++ with -O3 optimization — produces the serial baseline."*

```bash
cd src/openmp && g++ -O3 -fopenmp -o tsunami_omp tsunami_omp.cpp && cd ../..
```
> *"Same compiler, but `-fopenmp` tells g++ to recognise OpenMP pragmas and link the OpenMP runtime library."*

```bash
cd src/mpi && mpic++ -O3 -o tsunami_mpi tsunami_mpi.cpp && cd ../..
```
> *"`mpic++` is a wrapper around g++ that automatically includes the MPI headers and links the MPI library — saves us from writing the `-I` and `-L` paths manually."*

```bash
cd src/hybrid && mpic++ -O3 -fopenmp -o tsunami_hybrid tsunami_hybrid.cpp && cd ../..
```
> *"For Hybrid we need both — `mpic++` for MPI plus `-fopenmp` for the OpenMP layer."*

---

### Phase B — Run (50 sec)

```bash
cd src/serial && ./tsunami_serial && cd ../..
```
> *"Serial baseline runs first. Expect roughly 5 to 8 seconds on this machine."*

(wait for time to print)

> *"There it is — Serial took X seconds."*

```bash
cd src/openmp && ./tsunami_omp 1 && cd ../..
```
> *"OpenMP with **one thread**. This is the same code but running through the OpenMP runtime. Expect it to be slightly slower than serial because OpenMP has setup costs even with no actual parallelism."*

```bash
cd src/openmp && ./tsunami_omp 4 && cd ../..
```
> *"Now **four threads** — this is where speedup should appear. The grid loops are now divided across four CPU threads."*

```bash
cd src/mpi && mpiexec -n 4 ./tsunami_mpi && cd ../..
```
> *"**MPI with four processes.** `mpiexec` is the MPI launcher — it spins up 4 separate processes, each holding a quarter of the grid. They communicate via halo exchange every timestep."*

```bash
cd src/hybrid && mpiexec -n 2 ./tsunami_hybrid 4 && cd ../..
```
> *"Hybrid: **two MPI processes, four OpenMP threads each** — 8 cores total. Two-level parallelism."*

---

### Phase C — Dashboard (3 min)

**Alt-Tab to the browser tab with the dashboard.** Click **Refresh data** in the sidebar (if banner is showing).

> *"This is our visualisation dashboard. It's built in **Streamlit** — a Python framework for interactive data apps. It reads the binary output files our simulations write, and the benchmark CSV."*

**Click Serial radio button.**
> *"Serial implementation, latest run. The heatmap shows the wave height — red is positive, blue is negative — and we can scrub through timesteps with this slider. Watch how the initial disturbance propagates outward."*

(drag slider through 3–4 positions)

**Click OpenMP.**
> *"OpenMP. **Bit-identical wave to serial** — proving our parallel implementation is numerically correct. The only difference is execution time, shown here on the right."*

**Click MPI, then Hybrid.**
> *"Same for MPI and Hybrid. All four produce identical physics. They differ only in **how fast** they produce it."*

(Point to the timing panel)

> *"Hybrid was our fastest single configuration at this scale, but MPI-2 was our most **efficient** — meaning highest speedup per core."*

**Switch back to slides — Slide 8.**

---

## 3. PRESENTATION (continued)

### Slide 8 — Results: Execution Time (60 sec)

> *"Here's the full timing study — twelve configurations against the serial baseline."*

> *"**Key reading**: serial took 7.93 seconds. Our fastest configuration is MPI with 2 processes, at 4.16 seconds — a **1.91 times speedup**, or about 95% parallel efficiency on two cores."*

> *"Notice the anomaly — **OpenMP with 1 thread is slower than serial**. That's expected: OpenMP carries runtime overhead. With one thread there is no parallelism to amortize that overhead, so you pay the cost with no benefit. This is a textbook OpenMP finding."*

**Click → Slide 9.**

---

### Slide 9 — Results: Scaling Analysis (60 sec)

> *"Now plotting **speedup** versus number of cores."*

(point to the dashed ideal line)

> *"This dashed line is **ideal scaling** — perfect parallelization. Every real implementation falls below it."*

> *"Three findings:"*

> *"**First** — MPI-2 hits 1.91 times, sitting right under the ideal line. That's 95% efficient."*

> *"**Second** — past four cores, **all** curves plateau. This machine has only 4 physical cores; the 5-to-8 region is hyperthreading. Hyperthreads share floating-point execution units, and our stencil is FP-bound — so hyperthreading actually hurts here. This is a known result for memory-bandwidth-bound HPC codes."*

> *"**Third** — Hybrid does not outperform pure MPI on a single node. The two-level structure adds overhead without unlocking new hardware. On a real multi-node cluster Hybrid would dominate — here on one machine, pure MPI wins."*

**Click → Slide 10.**

---

### Slide 10 — Limitations (60 sec)

> *"We want to be honest about the limitations."*

(walk through the four cards)

> *"**One** — Memory-bandwidth ceiling. The 5-point stencil reads 5 doubles and writes 1 per cell. The arithmetic intensity is low, so we hit the bandwidth ceiling well before we run out of compute. The Roofline model and Hager-and-Wellein predict exactly the plateau we observe."*

> *"**Two** — Hyperthreading hurts. As mentioned, our code is FP-heavy and HT shares the FP unit."*

> *"**Three** — Run-to-run variance was around 10 to 20%. We didn't pin processes with `taskset`, and WSL's scheduler can migrate threads between cores. A production benchmark would use 3-run medians with CPU pinning."*

> *"**Four** — Our OpenMP creates a parallel region inside the time loop — that's 2000 barrier synchronizations. Moving the parallel region outside, with `#pragma omp for` inside, would reduce overhead and likely lift the OpenMP speedup curve."*

**Click → Slide 11.**

---

### Slide 11 — Conclusion (30 sec)

> *"To summarize. We built and benchmarked four implementations of a 2D wave-equation solver. We validated all parallel versions against serial — they are bit-identical. We measured 12 configurations, computed speedup and efficiency, and built an interactive dashboard for reproducibility."*

> *"Our best result was MPI with 2 processes — 1.91 times speedup at 95% efficiency."*

> *"Future work includes moving the OpenMP parallel region outside the time loop, adding SIMD vectorization, taskset pinning, and ultimately scaling to a real multi-node cluster."*

**Click → Slide 12.**

---

### Slide 12 — Thank You + References (10 sec)

> *"Thank you. We're happy to take questions."*

**Stay on this slide for Q&A.**

---

## 4. CODE DEEP-DIVE — answer "explain this file"

Sir will likely ask you to open one or more of the four `.cpp` files and walk through it. Here's the answer for each.

### 4.1 — `src/serial/tsunami_serial.cpp`

**Lines 1–7 (imports):** *"Standard C++ headers — `vector` for the grid arrays, `cmath` for sqrt/pow/exp, `fstream` for binary file output, `chrono` for timing."*

**Lines 9–16 (parameters):**
- `N = 1000` — grid is 1000×1000 cells.
- `L = 1.0` — physical domain size.
- `c = 1.0` — wave speed (non-dimensionalised).
- `dx = L / N` — spatial step.
- `dt = 0.0005` — time step. Chosen so Courant number `c·dt/dx = 0.5` — well within CFL stability limit of 1/√2.
- `STEPS = 2000` — total iterations.
- `OUTPUT_FREQ = 100` — save a snapshot every 100 steps (20 snapshots total).

**Lines 19–21 (idx function):** *"This is a flat-array indexing helper. We store the 2D grid as a 1D `std::vector<double>` for memory efficiency — `idx(y,x)` gives the flattened index."*

**Lines 25–27 (three arrays):** *"We need three time levels for the leap-frog scheme: `h_prev` is t-1, `h_curr` is t, `h_next` is t+1. After each step we rotate them with `std::swap`."*

**Lines 30–42 (initial condition):** *"A Gaussian disturbance centered in the domain — like a 'drop' creating an outgoing wave. We set `h_prev = h_curr` to enforce zero initial velocity — required for leap-frog startup."*

**Lines 48–66 (main time loop + stencil):** *"This is the core. For each interior cell we compute the 5-point Laplacian, multiply by Courant-squared, and update `h_next` using the leap-frog formula. Boundary cells are skipped — they're set separately."*

**Lines 70–75 (boundary conditions):** *"Dirichlet boundaries — edges held at h=0. This reflects the wave back with phase inversion, behaving as a fixed wall."*

**Lines 78–79 (pointer swap):** *"This is the **memory rotation trick**. Instead of copying entire arrays, we swap the pointers — O(1) instead of O(N²). Critical for performance."*

**Lines 82–90 (output):** *"Every OUTPUT_FREQ steps, dump the current grid to a binary file for the visualisation."*

**Common questions:**
- *"Why O3?"* → "Highest level of compiler optimization. Enables loop unrolling, vectorization, function inlining."
- *"Why double precision?"* → "Wave equations accumulate rounding errors over thousands of timesteps. Single precision would drift visibly."

---

### 4.2 — `src/openmp/tsunami_omp.cpp`

**Differences from serial:**

**Line 8:** `#include <omp.h>` — OpenMP runtime header.

**Lines 24–29:** *"We parse the thread count from `argv[1]` and call `omp_set_num_threads()` — letting us test different thread counts without recompiling."*

**Line 40:** `#pragma omp parallel for collapse(2)` (in the initial condition loop).
- *"This directive tells OpenMP to parallelize the next loop across threads."*
- *"`collapse(2)` merges the y and x loops into a single iteration space — gives better load balancing than just parallelizing the outer loop, especially when N is small relative to the thread count."*

**Line 59:** Same `#pragma omp parallel for collapse(2)` on the main stencil loop.

**Line 78:** `#pragma omp parallel for` on the boundary loop — only one loop, so no `collapse`.

**Common questions:**
- *"Why `collapse(2)`?"* → "Without it, only the outer `y` loop is parallelized — 1000 iterations across 4 threads is 250 each, fine. But `collapse(2)` turns it into 1,000,000 iterations distributed evenly. Better load balance for non-square thread counts."
- *"Race conditions?"* → "Each thread writes to a **different (y,x) cell** in `h_next`, so no race. Reads from `h_curr` and `h_prev` are read-only inside the parallel region — also safe."
- *"What about `h_next` being modified by multiple threads?"* → "Each thread owns its own subset of (y,x) indices — disjoint writes. No collision."

---

### 4.3 — `src/mpi/tsunami_mpi.cpp`

This is the most complex file. Be ready to explain it section by section.

**Line 1:** `#include <mpi.h>` — MPI runtime.

**Lines 22–27:** `MPI_Init`, `MPI_Comm_rank`, `MPI_Comm_size` — *"Initialise MPI, get my process rank (0 to size-1) and total process count."*

**Lines 29–37 (row distribution):**
- *"We split the N rows of the grid among `size` processes."*
- *"`row_counts[i]` = how many rows process i owns. We handle the case where N doesn't divide evenly by giving the first `N % size` processes one extra row."*

**Lines 39–45 (local arrays):**
- `local_rows = rows + 2` — *"each process holds its assigned rows **plus 2 halo rows** (one top, one bottom) for stencil access to neighbour data."*

**Lines 51–64 (initial condition):** *"Same Gaussian drop, but each process initialises **only its own slice**, using `global_y = global_start + y - 1` to map local row to the global grid."*

**Lines 83–84 (neighbours):**
- `top_neighbor = rank - 1` (or `MPI_PROC_NULL` for rank 0)
- `bottom_neighbor = rank + 1` (or `MPI_PROC_NULL` for last rank)
- *"`MPI_PROC_NULL` means 'no neighbour' — MPI silently skips sends/receives to it."*

**Lines 87–112 (update_row lambda):** *"The stencil update for a single row, applied to interior rows only."*

**Lines 114–129 (the main loop — KEY SECTION):**
```cpp
MPI_Irecv(...);  // receive top halo from above
MPI_Irecv(...);  // receive bottom halo from below
MPI_Isend(...);  // send our top row up
MPI_Isend(...);  // send our bottom row down

for (int y = 2; y <= rows - 1; y++) {
    update_row(y);          // compute interior rows while comm happens
}

MPI_Waitall(...);            // wait for halo data to arrive

update_row(1);               // now we can compute boundary rows
update_row(rows);
```

This is the **non-blocking halo exchange with computation-communication overlap.** Explain it like this:

> *"The trick is `Isend` and `Irecv` — non-blocking versions. They post the send and receive but **return immediately**, letting computation continue. We then update all **interior rows** that don't depend on halo data. Once we finish them, we `MPI_Waitall` for the halo data to arrive, then compute the **boundary rows** that needed it. This hides communication latency behind computation."*

**Lines 141–155 (output):** *"Only every 100 steps. We use `MPI_Gatherv` to collect all process slices to rank 0, which writes the full grid to a single binary file. `Gatherv` handles unequal slice sizes."*

**Lines 158–168 (timing):** *"Each process measures its own time. We take the **maximum** across all processes — that's the wall-clock time of the slowest process, which is what bounds the simulation."*

**Common questions:**
- *"Why non-blocking?"* → "To overlap communication with computation. Saves the time we'd otherwise spend waiting for halos to arrive."
- *"What's a halo row?"* → "A ghost row that holds a copy of the neighbour's edge row. Needed because the 5-point stencil reaches one cell up and one cell down — we need the row from the neighbour to compute our own boundary."
- *"What if the boundary process has only its physical edge above/below?"* → "We use `MPI_PROC_NULL` for the missing neighbour — MPI doesn't actually communicate. The `update_row` lambda also checks `global_y == 0 || N-1` and sets Dirichlet."

---

### 4.4 — `src/hybrid/tsunami_hybrid.cpp`

This is MPI + OpenMP combined.

**Line 26:** `MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);`
- *"This is the threaded MPI initializer. `MPI_THREAD_FUNNELED` means: multiple threads will exist per process, but only the main thread will call MPI functions. This is what we want — OpenMP threads do computation, only the main thread does MPI calls."*

**Lines 33–37:** *"Thread count from argv[1], same as the pure OpenMP version."*

**Lines 39–74:** *"Identical to the MPI version — distribute rows, allocate local arrays with halos, initialize with `#pragma omp parallel for collapse(2)` (line 61)."*

**Lines 141–144:**
```cpp
#pragma omp parallel for
for (int y = 2; y <= rows - 1; y++) {
    update_row(y);
}
```
*"This is the key hybrid line. We parallelize the interior-row loop across OpenMP threads — **inside** each MPI process. So MPI has split the grid into slices, and OpenMP further splits each slice across CPU threads."*

**Lines 148–163 (parallel sections):**
```cpp
#pragma omp parallel sections
{
    #pragma omp section { update_row(1); }
    #pragma omp section { update_row(rows); }
}
```
*"After the MPI_Waitall, we update the two boundary rows in parallel — one thread per row, since they're independent."*

**Common questions:**
- *"Why `MPI_THREAD_FUNNELED` and not `MPI_THREAD_MULTIPLE`?"* → "FUNNELED is enough — only the main thread calls MPI. MULTIPLE allows any thread to call MPI but adds locking overhead inside the MPI library. We don't need it."
- *"Is this the best hybrid configuration?"* → "On a single-node 4-core machine, no — pure MPI wins. Hybrid shines on multi-node clusters where MPI is between nodes (slow network) and OpenMP is within each node (fast shared memory)."

---

## 5. ANTICIPATED Q&A — quick-fire answers

| Question | One-line answer |
|----------|-----------------|
| *Why not real Shallow Water Equations?* | Linear surrogate is sufficient for deep-ocean propagation and is the standard HPC benchmark stencil; full SWE adds 3 coupled fields and Riemann solvers, out of scope for this course. |
| *What does `-O3` do?* | Highest compiler optimization — loop unrolling, auto-vectorization, function inlining. |
| *What's CFL?* | Courant–Friedrichs–Lewy condition (1928). For our scheme, `c·Δt/Δx ≤ 1/√2 ≈ 0.707` — bounds how big a timestep we can take. Ours is 0.5 — safe. |
| *Why second-order?* | Central differences cancel odd-order Taylor terms. Truncation error is O(Δt²+Δx²) — much better than first-order forward differences. |
| *What's reflective vs absorbing boundary?* | We use Dirichlet (h=0) which reflects with phase inversion. True non-reflective requires absorbing layers (PML, Mur, etc.) — not used here. |
| *Speedup formula?* | Speedup = T_serial / T_parallel. Efficiency = Speedup / cores. |
| *Why is MPI better than OpenMP at 2 cores?* | Separate memory spaces avoid OpenMP's cache-coherence traffic and false sharing. |
| *What's a halo row?* | Ghost row holding a copy of neighbour's edge data, needed because the 5-point stencil reaches one cell outside the local slice. |
| *Why non-blocking MPI?* | To overlap communication with computation — start the halo exchange, work on interior rows while it's in flight, then wait. |
| *What is `mpiexec`?* | The MPI launcher — spawns N processes of your binary and connects them via the MPI runtime. |
| *Why hybrid didn't win?* | On a single-node 4-core machine, MPI alone already saturates memory bandwidth. Hybrid would win on a multi-node cluster where MPI crosses slow networks. |
| *Hyperthreading?* | Two logical threads per physical core sharing the same FP unit. Helps for integer workloads (waiting on memory) but hurts FP-bound stencil code. |
| *What's a stencil?* | A computational pattern where each output cell depends on a fixed local neighbourhood of input cells. The 5-point stencil here uses 4 neighbours + self. |
| *Why memory-bandwidth-bound?* | Low arithmetic intensity: 5 doubles read + 1 written = 48 bytes per ~5 FLOPs ≈ 0.1 FLOP/byte. Modern CPUs need >1 FLOP/byte to be compute-bound. |
| *Future work?* | Move OMP parallel region outside time loop; SIMD vectorization; taskset pinning; multi-node cluster; full nonlinear SWE. |

---

## 6. RECOVERY — if something goes wrong

| Symptom | Recovery |
|---------|----------|
| Compile fails | `sudo apt install -y g++ openmpi-bin libopenmpi-dev` then retry. (Won't happen if you tested 15 min before.) |
| `mpiexec: not enough slots` | Add `--oversubscribe`: `mpiexec --oversubscribe -n 8 ...` |
| Run takes 30s instead of 5s | "Some background load — let me re-run." Don't apologise. |
| Dashboard shows blank | Click **Refresh data** in sidebar. |
| Dashboard crashes | `pkill streamlit; streamlit run src/dashboard/app.py` from terminal. |
| Sir asks something you don't know | *"That's a great question — let me think. [pause] Honestly I'm not sure, but my intuition is X because of Y. I'd want to verify that experimentally before committing to an answer."* — **better than bluffing.** |

---

## 7. CLOSING TIPS

- **Speak slowly.** You know this material. Pause between sentences. Confidence over speed.
- **Point at the screen.** Don't just talk at the slide — physically gesture at the formula or the chart you're discussing.
- **Let sir interrupt.** If he wants to dive into a file or ask about a number, *welcome it* — that's where you score the most marks.
- **When you don't know, say so.** Then offer your best guess with reasoning. Bluffing is worse than admitting uncertainty.
- **Have fun.** You built this. You understand it. You earned the right to talk about it.

Good luck.
