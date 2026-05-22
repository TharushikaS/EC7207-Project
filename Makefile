# =========================================================================
# EC7207 Tsunami HPC — Build & Benchmark Sweep
# =========================================================================
# Targets:
#   make            -> build everything that's available on this machine
#   make build      -> same as `make`
#   make bench      -> run the full best-of-3 sweep, write data/benchmarks.csv
#   make clean      -> remove binaries
#   make distclean  -> remove binaries + benchmark CSV
#
# CUDA is included automatically when `nvcc` is on PATH; otherwise the
# `cuda` targets are skipped with a warning so the rest of the sweep
# still runs on a CPU-only box.
# =========================================================================

# ----- Toolchain -----------------------------------------------------------
CXX        ?= g++
MPICXX     ?= mpic++
NVCC       ?= nvcc
MPIEXEC    ?= mpiexec

CXXFLAGS   ?= -O3
OMPFLAGS   ?= -fopenmp
NVCCFLAGS  ?= -O3

# Detect CUDA toolchain availability. If nvcc isn't installed we still want
# the rest of the build/bench to work — just print a notice and skip CUDA.
HAVE_NVCC  := $(shell command -v $(NVCC) 2>/dev/null)

# ----- Layout --------------------------------------------------------------
SRC_DIR    := src
DATA_DIR   := data
GT_DIR     := $(DATA_DIR)/ground_truth
BENCH_CSV  := $(DATA_DIR)/benchmarks.csv

SERIAL_BIN := $(SRC_DIR)/serial/tsunami_serial
OMP_BIN    := $(SRC_DIR)/openmp/tsunami_omp
MPI_BIN    := $(SRC_DIR)/mpi/tsunami_mpi
HYBRID_BIN := $(SRC_DIR)/hybrid/tsunami_hybrid
CUDA_BIN   := $(SRC_DIR)/cuda/tsunami_cuda

CPU_BINS   := $(SERIAL_BIN) $(OMP_BIN) $(MPI_BIN) $(HYBRID_BIN)
ifneq ($(HAVE_NVCC),)
ALL_BINS   := $(CPU_BINS) $(CUDA_BIN)
else
ALL_BINS   := $(CPU_BINS)
endif

# ----- Sweep configuration -------------------------------------------------
# Mirror the configurations documented in the README so the dashboard's
# "All implementations" view stays meaningful.
OMP_THREADS    := 1 2 4 8
MPI_PROCS      := 1 2 4 8
HYBRID_CONFIGS := 2x2 2x4 4x2
RUNS_PER_CFG   := 3   # best-of-N

# Allow extra MPI flags (e.g. --oversubscribe on machines without enough
# physical cores) without editing the Makefile:  make bench MPIFLAGS=--oversubscribe
MPIFLAGS       ?=

.PHONY: all build bench clean distclean help \
        serial openmp mpi hybrid cuda \
        bench-serial bench-openmp bench-mpi bench-hybrid bench-cuda

# =========================================================================
# Build
# =========================================================================
all: build

build: $(ALL_BINS)
ifeq ($(HAVE_NVCC),)
	@echo ">>> Note: nvcc not found — CUDA target skipped."
endif

serial: $(SERIAL_BIN)
openmp: $(OMP_BIN)
mpi:    $(MPI_BIN)
hybrid: $(HYBRID_BIN)
cuda:   $(CUDA_BIN)

$(SERIAL_BIN): $(SRC_DIR)/serial/tsunami_serial.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(OMP_BIN): $(SRC_DIR)/openmp/tsunami_omp.cpp
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) -o $@ $<

$(MPI_BIN): $(SRC_DIR)/mpi/tsunami_mpi.cpp
	$(MPICXX) $(CXXFLAGS) -o $@ $<

$(HYBRID_BIN): $(SRC_DIR)/hybrid/tsunami_hybrid.cpp
	$(MPICXX) $(CXXFLAGS) $(OMPFLAGS) -o $@ $<

$(CUDA_BIN): $(SRC_DIR)/cuda/tsunami_cuda.cu
ifeq ($(HAVE_NVCC),)
	@echo ">>> Skipping $@ — nvcc not found."
else
	$(NVCC) $(NVCCFLAGS) -o $@ $<
endif

# =========================================================================
# Benchmark sweep
# =========================================================================
# Strategy: each binary already prints its own wall-clock time on the line
#   "<...> Execution Time<...>: <seconds> seconds"
# We `grep | awk` that out, repeat RUNS_PER_CFG times, keep the minimum
# (best-of-N — filters out one-off OS noise), and append a CSV row.
#
# Each binary is run from its own directory so the relative output paths
# (../../data/ground_truth/) resolve correctly — same convention as the
# README's manual instructions.
#
# CSV schema matches what src/dashboard/app.py already consumes:
#   implementation,processes,threads,time_seconds

bench: build | $(GT_DIR)
	@echo "implementation,processes,threads,time_seconds" > $(BENCH_CSV)
	@$(MAKE) --no-print-directory bench-serial
	@$(MAKE) --no-print-directory bench-openmp
	@$(MAKE) --no-print-directory bench-mpi
	@$(MAKE) --no-print-directory bench-hybrid
ifneq ($(HAVE_NVCC),)
	@$(MAKE) --no-print-directory bench-cuda
else
	@echo ">>> Skipping CUDA bench — nvcc not found."
endif
	@echo ""
	@echo ">>> Benchmark sweep complete. Results: $(BENCH_CSV)"
	@cat $(BENCH_CSV)

$(GT_DIR):
	@mkdir -p $(GT_DIR)

# ----- Per-implementation sweep targets ------------------------------------
# `_run_min` is a shell helper that takes a command and runs it
# RUNS_PER_CFG times, extracting the seconds field and reporting the min.
# We inline it per-target rather than as a make-function to keep the rules
# easy to read.

# Extracts the seconds field from a line like
#   "Serial Execution Time: 4.27 seconds"
#   "Hybrid Execution Time (2 MPI processes x 4 OpenMP threads): 4.10881 seconds"
# by picking the token immediately before "seconds" — unambiguous across
# all five implementations, even when the line also contains other numbers
# (process counts, thread counts).
EXTRACT_TIME = grep -E "Execution Time" | awk '{for(i=1;i<=NF;i++) if($$i=="seconds"){print $$(i-1); exit}}'

# Reduce a whitespace-separated list of floats to its minimum.
MIN_OF       = awk '{m=$$1; for(i=2;i<=NF;i++) if($$i<m) m=$$i; print m}'

bench-serial: $(SERIAL_BIN)
	@echo ">>> serial (1 config)"
	@times=""; \
	for i in $$(seq 1 $(RUNS_PER_CFG)); do \
	    t=$$(cd $(SRC_DIR)/serial && ./tsunami_serial | $(EXTRACT_TIME)); \
	    times="$$times $$t"; \
	done; \
	best=$$(echo $$times | $(MIN_OF)); \
	echo "    runs:$$times -> best-of-$(RUNS_PER_CFG): $$best s"; \
	echo "serial,1,1,$$best" >> $(BENCH_CSV)

bench-openmp: $(OMP_BIN)
	@echo ">>> openmp (threads: $(OMP_THREADS))"
	@for n in $(OMP_THREADS); do \
	    times=""; \
	    for i in $$(seq 1 $(RUNS_PER_CFG)); do \
	        t=$$(cd $(SRC_DIR)/openmp && ./tsunami_omp $$n | $(EXTRACT_TIME)); \
	        times="$$times $$t"; \
	    done; \
	    best=$$(echo $$times | $(MIN_OF)); \
	    echo "    threads=$$n  runs:$$times -> best: $$best s"; \
	    echo "openmp,1,$$n,$$best" >> $(BENCH_CSV); \
	done

bench-mpi: $(MPI_BIN)
	@echo ">>> mpi (processes: $(MPI_PROCS))"
	@for p in $(MPI_PROCS); do \
	    times=""; \
	    for i in $$(seq 1 $(RUNS_PER_CFG)); do \
	        t=$$(cd $(SRC_DIR)/mpi && $(MPIEXEC) $(MPIFLAGS) -n $$p ./tsunami_mpi | $(EXTRACT_TIME)); \
	        times="$$times $$t"; \
	    done; \
	    best=$$(echo $$times | $(MIN_OF)); \
	    echo "    procs=$$p  runs:$$times -> best: $$best s"; \
	    echo "mpi,$$p,1,$$best" >> $(BENCH_CSV); \
	done

bench-hybrid: $(HYBRID_BIN)
	@echo ">>> hybrid (PxT configs: $(HYBRID_CONFIGS))"
	@for cfg in $(HYBRID_CONFIGS); do \
	    p=$$(echo $$cfg | cut -dx -f1); \
	    th=$$(echo $$cfg | cut -dx -f2); \
	    times=""; \
	    for i in $$(seq 1 $(RUNS_PER_CFG)); do \
	        t=$$(cd $(SRC_DIR)/hybrid && $(MPIEXEC) $(MPIFLAGS) -n $$p ./tsunami_hybrid $$th | $(EXTRACT_TIME)); \
	        times="$$times $$t"; \
	    done; \
	    best=$$(echo $$times | $(MIN_OF)); \
	    echo "    $${p}p x $${th}t  runs:$$times -> best: $$best s"; \
	    echo "hybrid,$$p,$$th,$$best" >> $(BENCH_CSV); \
	done

bench-cuda: $(CUDA_BIN)
	@echo ">>> cuda (1 config)"
	@times=""; \
	for i in $$(seq 1 $(RUNS_PER_CFG)); do \
	    t=$$(cd $(SRC_DIR)/cuda && ./tsunami_cuda | $(EXTRACT_TIME)); \
	    times="$$times $$t"; \
	done; \
	best=$$(echo $$times | $(MIN_OF)); \
	echo "    runs:$$times -> best-of-$(RUNS_PER_CFG): $$best s"; \
	echo "cuda,1,1,$$best" >> $(BENCH_CSV)

# =========================================================================
# Housekeeping
# =========================================================================
clean:
	@rm -f $(SERIAL_BIN) $(OMP_BIN) $(MPI_BIN) $(HYBRID_BIN) $(CUDA_BIN)
	@echo ">>> Removed binaries."

distclean: clean
	@rm -f $(BENCH_CSV)
	@echo ">>> Removed $(BENCH_CSV)."

help:
	@echo "EC7207 Tsunami HPC build & benchmark targets:"
	@echo "  make / make build   Build every available implementation"
	@echo "  make bench          Run best-of-$(RUNS_PER_CFG) sweep -> $(BENCH_CSV)"
	@echo "  make clean          Remove built binaries"
	@echo "  make distclean      Remove binaries + benchmark CSV"
	@echo ""
	@echo "Per-impl convenience targets: serial openmp mpi hybrid cuda"
	@echo "                              bench-serial bench-openmp bench-mpi"
	@echo "                              bench-hybrid bench-cuda"
	@echo ""
	@echo "Variables (override on the command line):"
	@echo "  RUNS_PER_CFG=$(RUNS_PER_CFG)   OMP_THREADS=\"$(OMP_THREADS)\""
	@echo "  MPI_PROCS=\"$(MPI_PROCS)\"   HYBRID_CONFIGS=\"$(HYBRID_CONFIGS)\""
	@echo "  MPIFLAGS=\"$(MPIFLAGS)\"   (e.g. --oversubscribe)"
