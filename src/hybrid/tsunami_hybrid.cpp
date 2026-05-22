#include <mpi.h>      // MPI library -> process-based parallelism
#include <omp.h>      // OpenMP library -> thread-based parallelism

#include <algorithm>  // std::swap
#include <cmath>      // sqrt, pow, exp
#include <cstdlib>    // atoi
#include <fstream>    // file writing
#include <iostream>   // console printing
#include <string>
#include <vector>

// ==========================================================
// SIMULATION PARAMETERS
// ==========================================================

// Grid size = 2000 x 2000
const int N = 2000;

// Physical size of the simulation area
const double L = 1.0;

// Wave speed
const double c = 1.0;

// Distance between neighboring cells
const double dx = L / N;

// Time step size. 2D CFL requires dt <= dx/(c*sqrt(2)) ≈ 3.54e-4 at N=2000;
// 2.5e-4 keeps CFL = 0.5 (same safety margin the project used at N=1000).
const double dt = 0.00025;

// Number of simulation timesteps
const int STEPS = 2000;

// Save simulation output every 100 steps
const int OUTPUT_FREQ = 100;

// ==========================================================
// Convert 2D coordinates -> 1D array index
// ==========================================================

inline int idx(int y, int x, int width) {
    return y * width + x;
}

int main(int argc, char* argv[]) {

    // ==========================================================
    // MPI INITIALIZATION
    // ==========================================================

    int provided = 0;

    // Initialize MPI with thread support
    // MPI_THREAD_FUNNELED:
    // only the main thread will make MPI calls
    MPI_Init_thread(&argc, &argv,
                    MPI_THREAD_FUNNELED,
                    &provided);

    int rank = 0; // ID of this MPI process
    int size = 1; // Total number of MPI processes

    // Get process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Get total process count
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // ==========================================================
    // OPENMP THREAD SETUP
    // ==========================================================

    // Default = 1 thread
    int num_threads = 1;

    // Read thread count from command line
    // Example:
    // ./program 4
    // -> use 4 OpenMP threads
    if (argc > 1) {
        num_threads = std::atoi(argv[1]);
    }

    // Set OpenMP thread count
    omp_set_num_threads(num_threads);

    // ==========================================================
    // DOMAIN DECOMPOSITION
    // Split grid rows among MPI processes
    // ==========================================================

    // Number of rows assigned to each process
    std::vector<int> row_counts(size, N / size);

    // Distribute remaining rows if N not divisible evenly
    for (int i = 0; i < N % size; i++) {
        row_counts[i]++;
    }

    // Starting row index for each process
    std::vector<int> row_offsets(size, 0);

    for (int i = 1; i < size; i++) {
        row_offsets[i] =
            row_offsets[i - 1] + row_counts[i - 1];
    }

    // Number of real rows owned by THIS process
    const int rows = row_counts[rank];

    // Global starting row index of THIS process
    const int global_start = row_offsets[rank];

    // Add 2 extra halo rows:
    // one top halo + one bottom halo
    const int local_rows = rows + 2;

    // ==========================================================
    // ALLOCATE LOCAL GRIDS
    // ==========================================================

    // Previous timestep
    std::vector<double> h_prev(local_rows * N, 0.0);

    // Current timestep
    std::vector<double> h_curr(local_rows * N, 0.0);

    // Next timestep
    std::vector<double> h_next(local_rows * N, 0.0);

    // ==========================================================
    // INITIAL WAVE SETUP
    // Create Gaussian bump in center
    // ==========================================================

    const int center_y = N / 2;
    const int center_x = N / 2;

    // Radius of initial disturbance
    const double drop_radius = N / 15.0;

    // Parallel initialization using OpenMP
    #pragma omp parallel for collapse(2)

    // Loop through this process's rows
    for (int y = 1; y <= rows; y++) {

        for (int x = 0; x < N; x++) {

            // Convert local row -> global row
            const int global_y = global_start + y - 1;

            // Distance from center of grid
            const double distance_squared =
                std::pow(x - center_x, 2) +
                std::pow(global_y - center_y, 2);

            // Create circular Gaussian wave
            if (std::sqrt(distance_squared)
                < drop_radius * 2) {

                h_curr[idx(y, x, N)] =
                    std::exp(
                        -distance_squared /
                        (drop_radius * drop_radius)
                    );

                // Initial velocity = 0
                h_prev[idx(y, x, N)] =
                    h_curr[idx(y, x, N)];
            }
        }
    }

    // ==========================================================
    // PREPARE GATHER INFORMATION
    // Used when rebuilding full grid on root process
    // ==========================================================

    std::vector<int> recv_counts(size);
    std::vector<int> displacements(size);

    for (int i = 0; i < size; i++) {

        // Number of elements received from each process
        recv_counts[i] = row_counts[i] * N;

        // Offset location in final gathered grid
        displacements[i] = row_offsets[i] * N;
    }

    // Full simulation grid exists only on rank 0
    std::vector<double> full_grid;

    if (rank == 0) {

        full_grid.resize(N * N);

        std::cout
            << "Starting Hybrid MPI+OpenMP Tsunami Simulation ("
            << N << "x" << N << ") with "
            << size << " MPI processes and "
            << num_threads
            << " OpenMP threads per process...\n";

        // Check MPI thread support
        if (provided < MPI_THREAD_FUNNELED) {

            std::cout
                << "Warning: MPI implementation "
                << "did not provide requested "
                << "thread support level.\n";
        }
    }

    // Synchronize all processes before timing
    MPI_Barrier(MPI_COMM_WORLD);

    // Start timer
    const double start_time = MPI_Wtime();

    // ==========================================================
    // MPI NEIGHBOR INFORMATION
    // ==========================================================

    // Rank above this process
    const int top_neighbor =
        (rank > 0) ? rank - 1 : MPI_PROC_NULL;

    // Rank below this process
    const int bottom_neighbor =
        (rank < size - 1)
        ? rank + 1
        : MPI_PROC_NULL;

    // Constant physics scaling factor
    const double factor =
        (c * c * dt * dt) / (dx * dx);

    // ==========================================================
    // FUNCTION TO UPDATE ONE ROW
    // ==========================================================

    auto update_row = [&](int y) {

        // Convert local row -> global row
        const int global_y =
            global_start + y - 1;

        // Fixed top/bottom boundaries
        if (global_y == 0 ||
            global_y == N - 1) {

            for (int x = 0; x < N; x++) {

                h_next[idx(y, x, N)] = 0.0;
            }

            return;
        }

        // Fixed left/right boundaries
        h_next[idx(y, 0, N)] = 0.0;
        h_next[idx(y, N - 1, N)] = 0.0;

        // Update interior cells
        for (int x = 1; x < N - 1; x++) {

            // 5-point stencil Laplacian
            const double laplacian =

                h_curr[idx(y + 1, x, N)] +
                h_curr[idx(y - 1, x, N)] +

                h_curr[idx(y, x + 1, N)] +
                h_curr[idx(y, x - 1, N)] -

                4.0 * h_curr[idx(y, x, N)];

            // Wave equation update
            h_next[idx(y, x, N)] =

                2.0 * h_curr[idx(y, x, N)]

                - h_prev[idx(y, x, N)]

                + factor * laplacian;
        }
    };

    // ==========================================================
    // MAIN TIME LOOP
    // ==========================================================

    for (int t = 0; t < STEPS; t++) {

        // ======================================================
        // HALO EXCHANGE (MPI COMMUNICATION)
        // ======================================================

        MPI_Request requests[4];

        // Receive top halo row
        MPI_Irecv(
            &h_curr[idx(0, 0, N)],
            N,
            MPI_DOUBLE,
            top_neighbor,
            1,
            MPI_COMM_WORLD,
            &requests[0]
        );

        // Receive bottom halo row
        MPI_Irecv(
            &h_curr[idx(rows + 1, 0, N)],
            N,
            MPI_DOUBLE,
            bottom_neighbor,
            0,
            MPI_COMM_WORLD,
            &requests[1]
        );

        // Send first owned row upward
        MPI_Isend(
            &h_curr[idx(1, 0, N)],
            N,
            MPI_DOUBLE,
            top_neighbor,
            0,
            MPI_COMM_WORLD,
            &requests[2]
        );

        // Send last owned row downward
        MPI_Isend(
            &h_curr[idx(rows, 0, N)],
            N,
            MPI_DOUBLE,
            bottom_neighbor,
            1,
            MPI_COMM_WORLD,
            &requests[3]
        );

        // ======================================================
        // COMPUTE INTERIOR ROWS
        // These rows do NOT need halo data yet
        // ======================================================

        #pragma omp parallel for

        for (int y = 2; y <= rows - 1; y++) {

            update_row(y);
        }

        // Wait until all MPI communication finishes
        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

        // ======================================================
        // COMPUTE BORDER ROWS
        // These depend on halo data
        // ======================================================

        #pragma omp parallel sections
        {

            // Top border row
            #pragma omp section
            {
                if (rows >= 1) {
                    update_row(1);
                }
            }

            // Bottom border row
            #pragma omp section
            {
                if (rows >= 2) {
                    update_row(rows);
                }
            }
        }

        // ======================================================
        // ROTATE TIME BUFFERS
        // ======================================================

        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);

        // ======================================================
        // SAVE OUTPUT
        // ======================================================

        if (t % OUTPUT_FREQ == 0) {

            // Gather all local grids into root process
            MPI_Gatherv(

                &h_curr[idx(1, 0, N)],

                rows * N,

                MPI_DOUBLE,

                full_grid.data(),

                recv_counts.data(),

                displacements.data(),

                MPI_DOUBLE,

                0,

                MPI_COMM_WORLD
            );

            // Root process writes output file
            if (rank == 0) {

                const std::string filename =

                    "../../data/ground_truth/hybrid_output_"

                    + std::to_string(t)

                    + ".bin";

                std::ofstream outfile(
                    filename,
                    std::ios::binary
                );

                if (outfile.is_open()) {

                    outfile.write(

                        reinterpret_cast<const char*>(
                            full_grid.data()
                        ),

                        full_grid.size()
                        * sizeof(double)
                    );
                }
            }
        }
    }

    // Synchronize all processes before ending timer
    MPI_Barrier(MPI_COMM_WORLD);

    // End timer
    const double end_time = MPI_Wtime();

    // Local execution time for this process
    const double local_time =
        end_time - start_time;

    double max_time = 0.0;

    // Find slowest process time
    MPI_Reduce(
        &local_time,
        &max_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );

    // Root process prints final timing
    if (rank == 0) {

        std::cout << "Simulation complete.\n";

        std::cout
            << "Hybrid Execution Time ("
            << size
            << " MPI processes x "
            << num_threads
            << " OpenMP threads): "
            << max_time
            << " seconds\n";
    }

    // Shut down MPI
    MPI_Finalize();

    return 0;
}