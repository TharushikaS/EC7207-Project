// =========================================================================
// CUDA Tsunami Wave Propagation
// =========================================================================
// GPU implementation of the same 2D linear wave equation solved by the
// serial / OpenMP / MPI / hybrid implementations. The numerical scheme,
// grid size, time step, and output cadence are identical, so the .bin
// frames it produces are bit-comparable with the other implementations.
//
// Parallelism model:
//   - One CUDA thread per interior cell of the grid
//   - 2D block decomposition with shared-memory tiling
//   - Each block loads a (BLOCK + 2) x (BLOCK + 2) halo tile into shared
//     memory, then every interior thread reads its 5-point stencil
//     neighbours from shared memory (no redundant global loads)
//   - h_prev / h_curr / h_next live on the device for the entire run;
//     we only DtoH-copy when it is time to write a snapshot
// =========================================================================

#include <cuda_runtime.h>

#include <algorithm>   // std::swap
#include <chrono>      // host-side timing
#include <cmath>       // exp, sqrt, pow
#include <cstdio>      // fprintf
#include <cstdlib>     // exit
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// --- Simulation Parameters (must match the other implementations) ----------
constexpr int    N           = 2000;     // Grid size (N x N)
constexpr double L           = 1.0;      // Physical domain length
constexpr double c_wave      = 1.0;      // Wave speed
constexpr double dx          = L / N;    // Spatial step
constexpr double dt          = 0.0005;   // Time step (CFL stable)
constexpr int    STEPS       = 2000;     // Total timesteps
constexpr int    OUTPUT_FREQ = 100;      // Snapshot cadence

// --- Block tile size --------------------------------------------------------
// 16 x 16 = 256 threads per block — a sweet spot that keeps occupancy high
// on every CUDA architecture from Pascal upward, and yields a shared-memory
// tile of 18 x 18 doubles = 2.5 KiB per block, well under the 48 KiB limit.
constexpr int BLOCK = 16;

// Minimal CUDA error-checking macro. We don't try to recover — if a CUDA
// call fails, the simulation is invalid, so we print and exit.
#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t _err = (call);                                             \
        if (_err != cudaSuccess) {                                             \
            std::fprintf(stderr, "CUDA error %s:%d: %s\n",                     \
                         __FILE__, __LINE__, cudaGetErrorString(_err));        \
            std::exit(EXIT_FAILURE);                                           \
        }                                                                      \
    } while (0)

// =========================================================================
// Stencil kernel
// =========================================================================
// One thread updates one interior cell. Threads inside a block cooperatively
// load an (BLOCK + 2) x (BLOCK + 2) tile of h_curr into shared memory; the
// outer ring of that tile is the halo. After __syncthreads(), every thread
// reads its 4 stencil neighbours straight from shared memory.
//
// Dirichlet (h = 0) boundary conditions are enforced by simply not writing
// to global boundary cells — those cells are pre-zeroed once on the host
// and never touched afterwards.
__global__ void stencil_kernel(const double* __restrict__ h_prev,
                               const double* __restrict__ h_curr,
                               double* __restrict__       h_next,
                               double                     factor)
{
    __shared__ double tile[BLOCK + 2][BLOCK + 2];

    // Global cell coordinates this thread is responsible for.
    const int gx = blockIdx.x * BLOCK + threadIdx.x;
    const int gy = blockIdx.y * BLOCK + threadIdx.y;

    // Local coordinates inside the shared-memory tile (offset by 1 for halo).
    const int lx = threadIdx.x + 1;
    const int ly = threadIdx.y + 1;

    // --- Center cell load ------------------------------------------------
    // Clamp to the grid so threads in over-hanging blocks still produce
    // valid shared-memory contents (those threads won't write to h_next).
    if (gx < N && gy < N) {
        tile[ly][lx] = h_curr[gy * N + gx];
    } else {
        tile[ly][lx] = 0.0;
    }

    // --- Halo loads -------------------------------------------------------
    // Top / bottom rows of the tile are loaded by the threads at the top
    // and bottom of the block; left / right columns are loaded by the
    // threads at the left and right edges. Out-of-grid halos read 0
    // (matches the Dirichlet h = 0 boundary).
    if (threadIdx.y == 0) {
        const int gy_top = gy - 1;
        tile[0][lx] = (gy_top >= 0 && gx < N) ? h_curr[gy_top * N + gx] : 0.0;
    }
    if (threadIdx.y == BLOCK - 1) {
        const int gy_bot = gy + 1;
        tile[BLOCK + 1][lx] =
            (gy_bot < N && gx < N) ? h_curr[gy_bot * N + gx] : 0.0;
    }
    if (threadIdx.x == 0) {
        const int gx_left = gx - 1;
        tile[ly][0] =
            (gx_left >= 0 && gy < N) ? h_curr[gy * N + gx_left] : 0.0;
    }
    if (threadIdx.x == BLOCK - 1) {
        const int gx_right = gx + 1;
        tile[ly][BLOCK + 1] =
            (gx_right < N && gy < N) ? h_curr[gy * N + gx_right] : 0.0;
    }

    __syncthreads();

    // Only interior cells get updated — boundary cells stay at 0 (Dirichlet).
    if (gx <= 0 || gx >= N - 1 || gy <= 0 || gy >= N - 1) return;

    const double laplacian =
        tile[ly + 1][lx] + tile[ly - 1][lx] +
        tile[ly][lx + 1] + tile[ly][lx - 1] -
        4.0 * tile[ly][lx];

    const double h_c = tile[ly][lx];
    const double h_p = h_prev[gy * N + gx];

    h_next[gy * N + gx] = 2.0 * h_c - h_p + factor * laplacian;
}

// =========================================================================
// Host driver
// =========================================================================
int main() {
    // --- Pick the first CUDA-capable device and print its name ----------
    int device_count = 0;
    CUDA_CHECK(cudaGetDeviceCount(&device_count));
    if (device_count == 0) {
        std::fprintf(stderr, "No CUDA-capable device found.\n");
        return EXIT_FAILURE;
    }
    cudaDeviceProp prop{};
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    CUDA_CHECK(cudaSetDevice(0));

    std::cout << "Starting CUDA Tsunami Simulation (" << N << "x" << N
              << ") on device 0 (" << prop.name << ")...\n";

    // --- Build the initial condition on the host ------------------------
    // Same Gaussian "drop" centered in the grid that every other
    // implementation uses. h_prev == h_curr ⇒ zero initial velocity.
    std::vector<double> h_host(N * N, 0.0);

    const int    center_y    = N / 2;
    const int    center_x    = N / 2;
    const double drop_radius = N / 15.0;

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const double dist2 =
                std::pow(x - center_x, 2) + std::pow(y - center_y, 2);
            if (std::sqrt(dist2) < drop_radius * 2.0) {
                h_host[y * N + x] =
                    std::exp(-dist2 / (drop_radius * drop_radius));
            }
        }
    }

    // --- Allocate the three time-level buffers on the device ------------
    const size_t bytes = static_cast<size_t>(N) * N * sizeof(double);
    double *d_prev = nullptr, *d_curr = nullptr, *d_next = nullptr;
    CUDA_CHECK(cudaMalloc(&d_prev, bytes));
    CUDA_CHECK(cudaMalloc(&d_curr, bytes));
    CUDA_CHECK(cudaMalloc(&d_next, bytes));

    // h_prev = h_curr  (zero initial velocity);  h_next zeroed so the
    // Dirichlet boundary cells stay at 0 forever (kernel never writes them).
    CUDA_CHECK(cudaMemcpy(d_curr, h_host.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_prev, h_host.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_next, 0, bytes));

    // --- Launch configuration -------------------------------------------
    const dim3 block(BLOCK, BLOCK);
    const dim3 grid((N + BLOCK - 1) / BLOCK, (N + BLOCK - 1) / BLOCK);

    const double factor = (c_wave * c_wave * dt * dt) / (dx * dx);

    // --- Time the whole compute phase, including any DtoH copies for I/O
    // We measure with cudaEvents so the GPU side is included and we don't
    // accidentally clip an outstanding kernel. The first event is recorded
    // after a sync to ensure prior copies are done.
    cudaEvent_t ev_start, ev_stop;
    CUDA_CHECK(cudaEventCreate(&ev_start));
    CUDA_CHECK(cudaEventCreate(&ev_stop));
    CUDA_CHECK(cudaDeviceSynchronize());
    CUDA_CHECK(cudaEventRecord(ev_start));

    // --- Main time loop -------------------------------------------------
    for (int t = 0; t < STEPS; ++t) {
        stencil_kernel<<<grid, block>>>(d_prev, d_curr, d_next, factor);

        // Rotate the time levels: prev <- curr, curr <- next.
        // Swapping device pointers on the host costs nothing — no copies.
        std::swap(d_prev, d_curr);
        std::swap(d_curr, d_next);

        if (t % OUTPUT_FREQ == 0) {
            // Pull the freshly-rotated current state back for serialization.
            // cudaMemcpy is synchronous on the default stream, so the kernel
            // above is implicitly finished before the copy begins.
            CUDA_CHECK(cudaMemcpy(h_host.data(), d_curr, bytes,
                                  cudaMemcpyDeviceToHost));

            const std::string filename =
                "../../data/ground_truth/cuda_output_"
                + std::to_string(t) + ".bin";
            std::ofstream outfile(filename, std::ios::binary);
            if (outfile.is_open()) {
                outfile.write(reinterpret_cast<const char*>(h_host.data()),
                              static_cast<std::streamsize>(bytes));
            }
        }
    }

    CUDA_CHECK(cudaEventRecord(ev_stop));
    CUDA_CHECK(cudaEventSynchronize(ev_stop));

    float elapsed_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, ev_start, ev_stop));
    const double elapsed = elapsed_ms / 1000.0;

    std::cout << "Simulation complete.\n";
    std::cout << "CUDA Execution Time: " << elapsed << " seconds\n";

    CUDA_CHECK(cudaEventDestroy(ev_start));
    CUDA_CHECK(cudaEventDestroy(ev_stop));
    CUDA_CHECK(cudaFree(d_prev));
    CUDA_CHECK(cudaFree(d_curr));
    CUDA_CHECK(cudaFree(d_next));

    return 0;
}
