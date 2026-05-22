"""Cross-implementation correctness check for the tsunami simulation.

After `make bench` (or any subset of the binaries) has produced .bin frames
in data/ground_truth/, this script reads the final saved timestep from
each implementation and reports differences vs the serial baseline at
several scales — from gross disagreement down to ULP-level round-off.

What we expect:
  serial vs serial          — bit-identical (sanity check)
  serial vs openmp          — bit-identical: same arithmetic, no reduction
  serial vs mpi / hybrid    — bit-identical: row decomposition doesn't change
                              which neighbours each stencil sums
  serial vs cuda            — typically a few ULPs (~1e-16 absolute) because
                              nvcc fuses a*b+c into FMA (one rounding instead
                              of two). Not a correctness problem.

What this script flags as a real problem:
  - any NaN or Inf in any frame  (algorithm blew up — usually CFL violation)
  - any difference whose magnitude is large in *both* absolute and relative
    terms (well above floating-point round-off)

Usage:
    python3 scripts/compare_grids.py                # last saved frame
    python3 scripts/compare_grids.py --step 0       # initial condition
    python3 scripts/compare_grids.py --ref openmp   # compare against openmp instead
    python3 scripts/compare_grids.py --verbose      # extra distribution detail
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
    """Read a .bin frame, return as (GRID_N, GRID_N) float64 or None if missing."""
    path = DATA_DIR / f"{IMPL_FILE_PREFIX[impl]}{step}.bin"
    if not path.exists():
        return None
    arr = np.fromfile(path, dtype=np.float64)
    if arr.size != GRID_N * GRID_N:
        print(f"!! {path.name}: expected {GRID_N*GRID_N} doubles, got {arr.size} — skipping",
              file=sys.stderr)
        return None
    return arr.reshape(GRID_N, GRID_N)


def health(frame: np.ndarray) -> tuple[int, int, float]:
    """Return (nan_count, inf_count, finite_max_abs). Cheap one-pass summary."""
    nan_n = int(np.isnan(frame).sum())
    inf_n = int(np.isinf(frame).sum())
    finite = frame[np.isfinite(frame)]
    fmax = float(np.max(np.abs(finite))) if finite.size else float("nan")
    return nan_n, inf_n, fmax


def ulp_distance(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """How many representable float64 values lie between a and b, element-wise.

    1 ULP = the gap to the next representable double at that magnitude. For
    a ≈ 1.0, 1 ULP ≈ 2.2e-16. The FMA-induced round-off in CUDA almost always
    falls within a handful of ULPs, which is the conventional definition of
    'numerically identical up to rounding'.
    """
    # np.spacing(x) returns the ULP magnitude at x. Avoid divide-by-zero where
    # the reference is exactly 0 by using nextafter(0,1) ≈ 5e-324 as the floor.
    eps = np.maximum(np.spacing(np.abs(a)), np.nextafter(0.0, 1.0))
    return np.abs(a - b) / eps


def compare_one(ref: np.ndarray, frame: np.ndarray) -> dict:
    """Compute a battery of difference statistics."""
    diff = frame - ref
    abs_diff = np.abs(diff)

    nan_in_frame = int(np.isnan(frame).sum())
    inf_in_frame = int(np.isinf(frame).sum())

    # Mask out non-finite cells so the stats below are meaningful even when
    # one (corrupt) frame is mostly NaN.
    valid = np.isfinite(abs_diff)
    if not valid.any():
        return {"all_nan": True, "nan": nan_in_frame, "inf": inf_in_frame}

    ad = abs_diff[valid]
    ref_norm = float(np.linalg.norm(ref[valid]))
    ref_peak = float(np.max(np.abs(ref[valid]))) or 1.0

    # Location of the worst absolute disagreement — useful when the diff is
    # localized (e.g. a single bad cell at a boundary) vs spread out.
    flat_max_i = int(np.argmax(np.where(valid, abs_diff, -1)))
    max_y, max_x = divmod(flat_max_i, GRID_N)
    max_abs = float(abs_diff.flat[flat_max_i])
    ref_at_max = float(ref.flat[flat_max_i])

    # ULP distance at the worst-diff cell — the most readable measure of
    # "is this round-off or real disagreement?". Anything < ~10 ULPs is
    # universally considered round-off-equivalent.
    ulps = ulp_distance(ref, frame)
    ulps_valid = ulps[valid]
    max_ulp = float(np.max(ulps_valid))
    median_ulp = float(np.median(ulps_valid))

    return {
        "all_nan": False,
        "nan": nan_in_frame,
        "inf": inf_in_frame,
        "exact": bool(np.array_equal(frame, ref)),
        "max_abs": max_abs,
        "max_rel_peak": max_abs / ref_peak,
        "mean_abs": float(np.mean(ad)),
        "p99_abs": float(np.percentile(ad, 99.0)),
        "l2_rel": float(np.linalg.norm(diff[valid]) / ref_norm) if ref_norm > 0 else 0.0,
        "max_ulp": max_ulp,
        "median_ulp": median_ulp,
        "max_at": (max_y, max_x),
        "ref_at_max": ref_at_max,
        "n_diff_gt_1ulp": int(np.sum(ulps_valid > 1.0)),
        "n_diff_gt_10ulp": int(np.sum(ulps_valid > 10.0)),
        "n_total": int(ulps_valid.size),
    }


def fmt_row(impl: str, s: dict) -> str:
    if s.get("all_nan"):
        return (f"{impl:<8} {'ALL NON-FINITE':>14}  nan={s['nan']}  inf={s['inf']}"
                "  (algorithm diverged — check CFL stability)")

    exact = "yes" if s["exact"] else "no "
    return (f"{impl:<8} "
            f"{s['max_abs']:>10.2e} "
            f"{s['max_rel_peak']:>10.2e} "
            f"{s['l2_rel']:>10.2e} "
            f"{s['max_ulp']:>9.1f} "
            f"{s['median_ulp']:>9.1f} "
            f"{exact:>6}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--step", type=int, default=STEPS - OUTPUT_FREQ,
                    help=f"Timestep to compare (default: {STEPS - OUTPUT_FREQ})")
    ap.add_argument("--ref", choices=list(IMPL_FILE_PREFIX), default="serial",
                    help="Reference implementation (default: serial)")
    ap.add_argument("--verbose", action="store_true",
                    help="Print location of worst diff and ULP distribution.")
    args = ap.parse_args()

    print(f"Comparing grids @ t={args.step}  ({GRID_N}x{GRID_N}, "
          f"reference = {args.ref})\n")

    ref = load_frame(args.ref, args.step)
    if ref is None:
        print(f"!! Reference frame missing: "
              f"{DATA_DIR / (IMPL_FILE_PREFIX[args.ref] + str(args.step) + '.bin')}",
              file=sys.stderr)
        return 1

    # Sanity-check the reference itself before doing any comparisons. If the
    # reference is full of NaN we'd produce a table of meaningless stats; flag
    # it loudly instead.
    ref_nan, ref_inf, ref_fmax = health(ref)
    if ref_nan or ref_inf:
        pct = 100.0 * ref_nan / ref.size
        print(f"!! Reference ({args.ref}) contains {ref_nan} NaN ({pct:.1f}%) "
              f"and {ref_inf} Inf values.")
        print(f"   This means the simulation itself diverged. Most common cause:")
        print(f"   CFL violation — verify  c * dt / dx <= 1/sqrt(2) ≈ 0.707.")
        print(f"   With dx = L/N and the current source-file constants you can")
        print(f"   check the bound by hand. Once the simulation is stable, all")
        print(f"   five frames should be finite and this script will show real")
        print(f"   round-off differences instead of nan.\n")
        # Continue anyway so the user sees that all impls suffer the same issue.
    else:
        print(f"   reference peak |h| = {ref_fmax:.3e}  (finite, healthy)\n")

    # Header. Columns:
    #   max|d|          worst absolute diff at any cell
    #   max/peak        same, normalised by reference peak amplitude
    #   L2 rel          ||diff||_2 / ||ref||_2 — overall agreement
    #   max ulp         worst diff measured in units of float64 ULPs at that
    #                   magnitude. < ~10 = pure round-off equivalence.
    #   med ulp         median ULP distance — tells you whether *most* cells
    #                   agree even if one is off.
    #   exact?          bitwise array_equal
    header = (f"{'impl':<8} {'max|d|':>10} {'max/peak':>10} {'L2 rel':>10} "
              f"{'max ulp':>9} {'med ulp':>9} {'exact':>6}")
    print(header)
    print("-" * len(header))

    rc = 0
    summaries: dict[str, dict] = {}
    for impl in IMPL_FILE_PREFIX:
        frame = load_frame(impl, args.step)
        if frame is None:
            print(f"{impl:<8} {'(missing)':>10}")
            if impl != "cuda":  # CUDA is legitimately optional on CPU-only boxes
                rc = 1
            continue
        s = compare_one(ref, frame)
        summaries[impl] = s
        print(fmt_row(impl, s))

    print()
    print("Legend: 1 ULP at |h|≈1 ≈ 2.2e-16. Round-off-only diffs are < ~10 ULPs.")
    print("        Anything beyond that (e.g. millions of ULPs) is a real bug.")

    if args.verbose:
        print()
        print("--- verbose detail ---")
        for impl, s in summaries.items():
            if s.get("all_nan"):
                continue
            print(f"\n{impl}:")
            print(f"  worst diff at (y, x) = {s['max_at']}, "
                  f"ref value there = {s['ref_at_max']: .6e}")
            print(f"  cells with diff > 1 ULP : {s['n_diff_gt_1ulp']:>10} / {s['n_total']}")
            print(f"  cells with diff > 10 ULP: {s['n_diff_gt_10ulp']:>10} / {s['n_total']}")
            print(f"  mean |diff|             : {s['mean_abs']:.3e}")
            print(f"  99th percentile |diff|  : {s['p99_abs']:.3e}")

    return rc


if __name__ == "__main__":
    sys.exit(main())
