"""Tsunami Wave HPC Demo Dashboard — EC7207 Group 48.

Single-page Streamlit app: pick an implementation, watch its wave animate,
see how long it took, and compare against the others.

Run with:
    streamlit run src/dashboard/app.py
"""

from __future__ import annotations

import time
from pathlib import Path

import numpy as np
import pandas as pd
import plotly.express as px
import streamlit as st

# --- Configuration ----------------------------------------------------------

GRID_N = 2000
STEPS = 2000
OUTPUT_FREQ = 100
FRAME_STEPS = list(range(0, STEPS, OUTPUT_FREQ))

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DATA_DIR = PROJECT_ROOT / "data" / "ground_truth"
BENCH_CSV = PROJECT_ROOT / "data" / "benchmarks.csv"

IMPL_FILE_PREFIX = {
    "serial": "output_",
    "openmp": "omp_output_",
    "mpi":    "mpi_output_",
    "hybrid": "hybrid_output_",
    "cuda":   "cuda_output_",
}
IMPL_LABEL = {
    "serial": "Serial",
    "openmp": "OpenMP",
    "mpi":    "MPI",
    "hybrid": "Hybrid",
    "cuda":   "CUDA",
}

st.set_page_config(page_title="Tsunami HPC Demo — Group 48", layout="wide")


# --- Data loading -----------------------------------------------------------

@st.cache_data(show_spinner=False)
def load_frame(impl: str, t: int) -> np.ndarray:
    path = DATA_DIR / f"{IMPL_FILE_PREFIX[impl]}{t}.bin"
    if path.exists():
        try:
            return np.fromfile(path, dtype=np.float64).reshape(GRID_N, GRID_N)
        except Exception:
            pass
    return _synthetic_frame(t)


def _synthetic_frame(t: int) -> np.ndarray:
    y, x = np.meshgrid(np.linspace(-1, 1, GRID_N), np.linspace(-1, 1, GRID_N))
    r = np.sqrt(x * x + y * y)
    phase = t * 0.05
    envelope = np.exp(-((r - phase * 0.3) ** 2) / 0.05)
    return envelope * np.cos(10 * r - phase)


@st.cache_data(show_spinner=False)
def load_benchmarks() -> tuple[pd.DataFrame, bool]:
    if BENCH_CSV.exists():
        try:
            return _augment(pd.read_csv(BENCH_CSV)), True
        except Exception:
            pass
    mock = pd.DataFrame(
        [
            ("serial", 1, 1, 12.34),
            ("openmp", 1, 4, 3.31),
            ("mpi",    4, 1, 3.40),
            ("hybrid", 2, 4, 0.95),
            ("cuda",   1, 1, 0.18),
        ],
        columns=["implementation", "processes", "threads", "time_seconds"],
    )
    return _augment(mock), False


def _augment(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df["cores"] = df["processes"] * df["threads"]
    t_serial = float(df.loc[df["implementation"] == "serial", "time_seconds"].iloc[0])
    df["speedup"] = t_serial / df["time_seconds"]
    df["efficiency"] = df["speedup"] / df["cores"]
    return df


def best_row(bench: pd.DataFrame, impl: str) -> pd.Series:
    """Best (fastest) configuration for a given implementation."""
    return bench[bench["implementation"] == impl].sort_values("time_seconds").iloc[0]


def data_is_real() -> bool:
    return any((DATA_DIR / f"{p}0.bin").exists() for p in IMPL_FILE_PREFIX.values())


# --- UI ---------------------------------------------------------------------

def main() -> None:
    bench, bench_is_real = load_benchmarks()
    frames_are_real = data_is_real()

    st.title("Tsunami Wave Propagation — HPC Demo")
    st.caption("EC7207 · Group 48 · 2D linear wave equation, four parallel implementations")

    if not (frames_are_real and bench_is_real):
        missing = []
        if not frames_are_real:
            missing.append("simulation frames (`data/ground_truth/*.bin`)")
        if not bench_is_real:
            missing.append("benchmark timings (`data/benchmarks.csv`)")
        st.info("Showing **mock data** for: " + ", ".join(missing)
                + ". Drop in real files and click *Refresh* to update.")
        if st.button("Refresh data"):
            st.cache_data.clear()
            st.rerun()

    impl = st.radio(
        "Implementation",
        options=list(IMPL_FILE_PREFIX),
        format_func=lambda k: IMPL_LABEL[k],
        horizontal=True,
    )

    left, right = st.columns([3, 2])

    with left:
        st.subheader(f"{IMPL_LABEL[impl]} — Wave Animation")
        t_index = st.select_slider(
            "Timestep", options=FRAME_STEPS, value=FRAME_STEPS[len(FRAME_STEPS) // 2],
        )
        play = st.button("▶ Play through all frames")
        wave_slot = st.empty()

    with right:
        st.subheader("Performance")
        row = best_row(bench, impl)
        st.metric("Execution time", f"{row['time_seconds']:.2f} s")
        st.metric("Speedup vs Serial", f"{row['speedup']:.2f} x")
        st.metric("Parallel efficiency", f"{row['efficiency'] * 100:.0f} %")
        st.metric("Cores used", f"{int(row['cores'])}  ({int(row['processes'])}p x {int(row['threads'])}t)")

        st.markdown("---")
        st.markdown("**All implementations**")
        summary = (
            bench.sort_values("time_seconds")
            .groupby("implementation", as_index=False)
            .first()
        )
        summary["Implementation"] = summary["implementation"].map(IMPL_LABEL)
        summary["Time (s)"] = summary["time_seconds"].map(lambda x: f"{x:.2f}")
        summary["Speedup"] = summary["speedup"].map(lambda x: f"{x:.2f}x")
        st.dataframe(
            summary[["Implementation", "Time (s)", "Speedup"]],
            hide_index=True, use_container_width=True,
        )

    # Render the wave (after both columns exist so it spans the left col).
    def render_wave(t: int) -> None:
        frame = load_frame(impl, t)
        vmax = float(np.max(np.abs(frame))) or 1e-9
        fig = px.imshow(
            frame,
            color_continuous_scale="RdBu_r",
            zmin=-vmax, zmax=vmax,
            origin="lower", aspect="equal",
        )
        # Disable interpolation — render each grid cell as a crisp pixel.
        fig.update_traces(zsmooth=False)
        fig.update_layout(
            margin=dict(l=0, r=0, t=30, b=0),
            title=f"t = {t} / {STEPS}",
            coloraxis_colorbar=dict(thickness=12),
            height=600,
            plot_bgcolor="white",
        )
        fig.update_xaxes(showticklabels=False, showgrid=False, zeroline=False)
        fig.update_yaxes(showticklabels=False, showgrid=False, zeroline=False)
        wave_slot.plotly_chart(
            fig,
            use_container_width=True,
            key=f"wave_{t}",
            config={"displaylogo": False, "toImageButtonOptions": {"scale": 2}},
        )

    if play:
        for t in FRAME_STEPS:
            render_wave(t)
            time.sleep(0.3)
    else:
        render_wave(t_index)


if __name__ == "__main__":
    main()
