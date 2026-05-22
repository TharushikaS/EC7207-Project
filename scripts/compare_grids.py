"""Cross-implementation correctness check for the tsunami simulation.

After `make bench` has produced .bin frames for each implementation, compare
their final saved timestep against the serial baseline and print a small
table of differences.

The five implementations should produce *bit-identical* results in theory
(same algorithm, same float64 arithmetic, same operation order). In practice:
- Serial / OpenMP — bit-identical (the omp_for over a fixed stencil
  reorders nothing that the FPU cares about).
- MPI / Hybrid — bit-identical too; row decomposition doesn't change which
  values are summed where.
- CUDA — may differ at the last few bits because nvcc's FMA contraction
  fuses (a*b + c) into a single fused-multiply-add. The fma op rounds once
  instead of twice, so individual cell values can be off by ~1 ULP (~1e-16
  in absolute terms for typical wave amplitudes). This is expected and not
  a correctness problem.

Usage:
    python3 scripts/compare_grids.py           # compares last frame (t=1900)
    python3 scripts/compare_grids.py --step 0  # compares initial condition
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

GRID_N = 2000
STEPS = 2000
OUTPUT_FREQ = 100

IMPL_FILE_PREFIX = {
    "serial": "output_",
    "openmp": "omp_output_",
    "mpi":    "mpi_output_",
    "hybrid": "hybrid_output_",
    "cuda":   "cuda_output_",
}

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = PROJECT_ROOT / "data" / "ground_truth"


def load_frame(impl: str, step: int) -> np.ndarray | None:
    path = DATA_DIR / f"{IMPL_FILE_PREFIX[impl]}{step}.bin"
    if not path.exists():
        return None
    return np.fromfile(path, dtype=np.float64).reshape(GRID_N, GRID_N)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--step", type=int, default=STEPS - OUTPUT_FREQ,
                    help=f"Timestep to compare (default: {STEPS - OUTPUT_FREQ}, "
                         f"the last saved frame)")
    args = ap.parse_args()
    step = args.step

    print(f"Comparing final grids @ t={step} (grid {GRID_N}x{GRID_N})\n")

    ref = load_frame("serial", step)
    if ref is None:
        print(f"!! Serial reference frame missing — "
              f"{DATA_DIR / (IMPL_FILE_PREFIX['serial'] + str(step) + '.bin')}",
              file=sys.stderr)
        print("   Run `make bench` (or at least the serial binary) first.",
              file=sys.stderr)
        return 1

    ref_norm = float(np.linalg.norm(ref))
    ref_max = float(np.max(np.abs(ref))) or 1.0

    # Pretty table. Columns:
    #   max |diff|         worst-case absolute error
    #   max |diff| / |ref| same, normalised by peak amplitude (dimensionless)
    #   L2 rel             ||a-b||_2 / ||ref||_2  (overall agreement)
    #   exact?             yes if bitwise identical
    header = f"{'impl':<8} {'max|diff|':>12} {'max/peak':>10} {'L2 rel':>12} {'exact?':>8}"
    print(header)
    print("-" * len(header))

    rc = 0
    for impl in IMPL_FILE_PREFIX:
        frame = load_frame(impl, step)
        if frame is None:
            print(f"{impl:<8} {'(missing)':>12}")
            if impl != "cuda":
                # CUDA legitimately missing on CPU-only boxes; others shouldn't be.
                rc = 1
            continue

        diff = frame - ref
        max_abs = float(np.max(np.abs(diff)))
        l2_rel = float(np.linalg.norm(diff) / ref_norm) if ref_norm > 0 else 0.0
        exact = "yes" if np.array_equal(frame, ref) else "no"

        print(f"{impl:<8} {max_abs:>12.3e} {max_abs/ref_max:>10.3e} "
              f"{l2_rel:>12.3e} {exact:>8}")

    print()
    print("All values < ~1e-13 indicate numerical agreement (round-off only).")
    print("CUDA may show ~1e-16 differences due to FMA contraction — expected.")
    return rc


if __name__ == "__main__":
    sys.exit(main())
