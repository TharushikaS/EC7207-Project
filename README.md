# High-Performance Simulation of Tsunami Wave Propagation

**EC7207 - High Performance Computing**
**- Group 48** 

| Reg. No. | Name |
|----------|------|
| EG/2021/4820 | Surasinghe R.L.D.T.H. |
| EG/2021/4823 | Tamasha A.P.D. |
| EG/2021/4825 | Tharshihan R.G. |

---

## Overview

This project simulates the propagation of a tsunami wave on a 2D ocean surface and uses it as a benchmark to compare four parallel-computing paradigms: serial, shared-memory (OpenMP), distributed-memory (MPI), and a hybrid MPI + OpenMP implementation.

The same physical model is solved by all four implementations. They produce bit-identical wave fields; what differs is wall-clock execution time, parallel speedup, and parallel efficiency.

## Mathematical Model

We solve the **2D linear wave equation**:

$$\frac{\partial^2 h}{\partial t^2} = c^2 \left( \frac{\partial^2 h}{\partial x^2} + \frac{\partial^2 h}{\partial y^2} \right)$$

where `h(x, y, t)` is the water surface elevation and `c = √(gH)` is the long-wave celerity. This equation is the small-amplitude, constant-depth linearization of the Shallow Water Equations (Pedlosky 1987 §3.9) and is the standard model for the deep-ocean propagation phase of a tsunami.

### Numerical Scheme

The PDE is discretized using second-order central differences in time (**leap-frog**) and a **5-point Laplacian stencil** in space, giving the explicit update rule:

```
h[i,j]^(n+1) = 2·h[i,j]^n − h[i,j]^(n−1) + (c·Δt/Δx)² · [ h[i+1,j] + h[i−1,j] + h[i,j+1] + h[i,j−1] − 4·h[i,j] ]^n
```

- Accuracy: `O(Δt² + Δx²)` — second-order in both space and time
- Stability: `c·Δt/Δx ≤ 1/√2 ≈ 0.707` (CFL condition, Courant–Friedrichs–Lewy 1928)
- Boundary conditions: Dirichlet (`h = 0`) — fixed-wall reflective

**Default parameters** (configured in each `.cpp` file):

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `N` | 1000 | Grid size (N × N cells) |
| `L` | 1.0 | Physical domain length |
| `c` | 1.0 | Wave speed |
| `dx` | 0.001 | Spatial step (L / N) |
| `dt` | 0.0005 | Time step (Courant = 0.5, stable) |
| `STEPS` | 2000 | Total timesteps |
| `OUTPUT_FREQ` | 100 | Frame save interval |

## Implementations

| Implementation | Source | Parallelism |
|----------------|--------|-------------|
| Serial | [src/serial/tsunami_serial.cpp](src/serial/tsunami_serial.cpp) | Single-threaded baseline (reference for correctness) |
| OpenMP | [src/openmp/tsunami_omp.cpp](src/openmp/tsunami_omp.cpp) | Shared-memory threads (`#pragma omp parallel` outside the time loop, `omp for` and `omp single` inside) |
| MPI | [src/mpi/tsunami_mpi.cpp](src/mpi/tsunami_mpi.cpp) | Distributed-memory processes with non-blocking halo exchange (`MPI_Isend` / `MPI_Irecv`) and compute–communication overlap |
| Hybrid | [src/hybrid/tsunami_hybrid.cpp](src/hybrid/tsunami_hybrid.cpp) | MPI between processes + OpenMP within each process (two-level parallelism, `MPI_THREAD_FUNNELED`) |
| CUDA   | [src/cuda/tsunami_cuda.cu](src/cuda/tsunami_cuda.cu) | GPU accelerator: one thread per cell, 16×16 blocks with shared-memory halo tiling so each stencil neighbour is read from on-chip memory |

---

## Prerequisites

This project is designed for a Linux / WSL2 environment with the following installed:

```bash
sudo apt update
sudo apt install -y g++ openmpi-bin libopenmpi-dev python3-pip python3-venv bc
```

Verify the toolchain:

```bash
g++ --version       # GCC 11+ recommended
mpic++ --version    # MPI compiler wrapper (OpenMPI 4.x)
mpiexec --version   # MPI launcher
```

The CUDA implementation is optional and only needed if you have an NVIDIA
GPU. Install the CUDA Toolkit (12.x recommended) and verify with:

```bash
nvcc --version      # NVCC compiler
nvidia-smi          # Driver + GPU visible
```

## Build

Compile all four implementations from the project root:

```bash
cd src/serial   && g++ -O3 -o tsunami_serial tsunami_serial.cpp                && cd ../..
cd src/openmp   && g++ -O3 -fopenmp -o tsunami_omp tsunami_omp.cpp             && cd ../..
cd src/mpi      && mpic++ -O3 -o tsunami_mpi tsunami_mpi.cpp                   && cd ../..
cd src/hybrid   && mpic++ -O3 -fopenmp -o tsunami_hybrid tsunami_hybrid.cpp    && cd ../..
cd src/cuda     && nvcc -O3 -o tsunami_cuda tsunami_cuda.cu                    && cd ../..
```

Compiler flags:
- `-O3` — maximum optimization (loop unrolling, auto-vectorization, inlining)
- `-fopenmp` — enables OpenMP pragmas and links the OpenMP runtime
- `mpic++` — wrapper around `g++` that includes the MPI headers/libraries
- `nvcc` — NVIDIA CUDA compiler; pass `-arch=sm_XX` to target a specific GPU
  (e.g. `-arch=sm_75` for Turing, `-arch=sm_86` for Ampere) if you need to
  override the default device architecture

## Run

Each binary writes snapshot files to `data/ground_truth/` relative to its own folder, so run each from inside its own directory:

```bash
# Serial
cd src/serial && ./tsunami_serial && cd ../..

# OpenMP with N threads
cd src/openmp && ./tsunami_omp 4 && cd ../..

# MPI with N processes
cd src/mpi    && mpiexec -n 4 ./tsunami_mpi && cd ../..

# Hybrid: P MPI processes × T OpenMP threads per process
cd src/hybrid && mpiexec -n 2 ./tsunami_hybrid 4 && cd ../..

# CUDA (single GPU)
cd src/cuda   && ./tsunami_cuda && cd ../..
```

If `mpiexec` complains about not enough slots, add `--oversubscribe`:

```bash
mpiexec --oversubscribe -n 8 ./tsunami_mpi
```

Each binary prints its measured wall-clock time, e.g.:

```
Hybrid Execution Time (2 MPI processes x 4 OpenMP threads): 4.10881 seconds
```

## Benchmark Sweep

To produce the full scaling study (`data/benchmarks.csv`) with averaging:

```bash
./scripts/run_benchmarks_v2.sh
```

This runs **each configuration three times** and records the minimum time (best-case, removes one-off OS noise). The full sweep covers:

- Serial — 1 configuration
- OpenMP — 1, 2, 4, 8 threads
- MPI — 1, 2, 4, 8 processes
- Hybrid — 2×2, 2×4, 4×2 (processes × threads)

Total runtime: ~10–15 minutes unattended.

## Visualization Dashboard

An interactive Streamlit dashboard at [src/dashboard/app.py](src/dashboard/app.py) loads the simulation outputs and benchmark CSV.

Setup (one time):

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r src/dashboard/requirements.txt
```

Launch:

```bash
source venv/bin/activate
streamlit run src/dashboard/app.py
```

The dashboard opens at `http://localhost:8501` and lets you:

- Pick an implementation and watch the wave animation
- Scrub through timesteps with a slider, play the full sequence
- See execution time, speedup vs. serial, and parallel efficiency
- Compare all four implementations in a single table

A simpler matplotlib-based 3D animation is also available:

```bash
cd scripts && python3 visualize.py
```

## Project Structure

```
EC7207-Project/
├── README.md
├── DEMO_RUNBOOK.md              # Step-by-step live-demo script
├── PRESENTER_GUIDE.md           # Full presenter's guide with code deep-dive
├── EC7207_Presentation.pptx     # 12-slide presentation
├── data/
│   ├── benchmarks.csv           # Timing results (produced by run_benchmarks_v2.sh)
│   └── ground_truth/            # *.bin frames from each implementation
├── scripts/
│   ├── run_benchmarks.sh        # Single-run benchmark sweep
│   ├── run_benchmarks_v2.sh     # Averaged (best-of-3) benchmark sweep
│   ├── generate_slides.js       # pptxgenjs script that builds the deck
│   └── visualize.py             # Matplotlib 3D wave animation
└── src/
    ├── serial/tsunami_serial.cpp
    ├── openmp/tsunami_omp.cpp
    ├── mpi/tsunami_mpi.cpp
    ├── hybrid/tsunami_hybrid.cpp
    ├── cuda/tsunami_cuda.cu
    └── dashboard/
        ├── app.py               # Streamlit dashboard
        └── requirements.txt
```

## Results Summary

Measured on a 4-physical-core (8-logical with hyperthreading) x86 CPU under WSL2 / Ubuntu / OpenMPI 4.1.6 / GCC 13.3, best-of-3 averaging:

| Configuration | Time (s) | Speedup |
|---------------|----------|---------|
| Serial | 4.27 | 1.00× |
| OpenMP 1 thread | 5.64 | 0.76× |
| OpenMP 2 threads | 4.06 | 1.05× |
| OpenMP 4 threads | 4.12 | 1.04× |
| OpenMP 8 threads | 5.48 | 0.78× |
| MPI 1 process | 4.51 | 0.95× |
| **MPI 2 processes** | **3.91** | **1.09×** ← best |
| MPI 4 processes | 4.13 | 1.03× |
| MPI 8 processes | 4.85 | 0.88× |
| Hybrid 2×2 | 4.11 | 1.04× |
| Hybrid 2×4 | 4.31 | 0.99× |
| Hybrid 4×2 | 4.58 | 0.93× |

**Key finding — Roofline ceiling.** Peak speedup is 1.09× because a single thread already saturates this CPU's memory bandwidth at N = 1000. The stencil has low arithmetic intensity (~0.1 FLOP/byte), making the workload memory-bandwidth-bound rather than compute-bound — exactly the regime where adding more cores cannot help. Roofline analysis confirms we are at the hardware ceiling, not a software bottleneck (Williams, Waterman & Patterson 2009; Hager & Wellein 2010, Ch. 5).

**Secondary findings.**
- OpenMP-1 is slower than serial — pure runtime overhead with no parallelism to amortize it
- All implementations regress at 8 threads/processes — hyperthreads share floating-point units in this FP-bound code
- MPI consistently slightly faster than OpenMP at low core counts — separate memory spaces avoid cache-coherence traffic

**Future work to break the ceiling.** Temporal cache-blocking (compute multiple timesteps per data load), SIMD vectorization with `#pragma omp simd`, NUMA-aware data placement, and scaling to a multi-node cluster where MPI's distributed-memory advantage becomes visible.

## References

1. Courant, R., Friedrichs, K., Lewy, H. (1928). *Über die partiellen Differenzengleichungen der mathematischen Physik.* Math. Ann. 100, 32–74. — original CFL stability paper.
2. Whitham, G. B. (1974). *Linear and Nonlinear Waves.* Wiley.
3. Pedlosky, J. (1987). *Geophysical Fluid Dynamics*, 2nd ed. Springer. — §3.9 derives the linear wave equation as the linearization of the Shallow Water Equations.
4. Titov, V. V. & Synolakis, C. E. (1998). Numerical modeling of tidal wave runup. *J. Waterway, Port, Coastal & Ocean Eng.* 124(4), 157–171. — MOST tsunami model.
5. LeVeque, R. J. (2002). *Finite Volume Methods for Hyperbolic Problems.* Cambridge.
6. Strikwerda, J. C. (2004). *Finite Difference Schemes and Partial Differential Equations*, 2nd ed. SIAM.
7. Williams, S., Waterman, A. & Patterson, D. (2009). Roofline: an insightful visual performance model for multicore architectures. *Communications of the ACM* 52(4), 65–76.
8. Hager, G. & Wellein, G. (2010). *Introduction to High Performance Computing for Scientists and Engineers.* CRC Press. — Ch. 5 covers memory-bandwidth analysis for stencil codes.

## License

Academic project — EC7207 High Performance Computing, 2026.



./scripts/run_benchmarks_v2.sh
source venv/bin/activate
streamlit run src/dashboard/app.py
