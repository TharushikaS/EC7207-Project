#include <mpi.h>          // MPI library for distributed parallelism
#include <algorithm>      // std::swap
#include <cmath>          // math functions
#include <fstream>        // file writing
#include <iostream>       // console output
#include <string>
#include <vector>

// ---------------- SIMULATION PARAMETERS ----------------

// Grid size: 2000 x 2000
const int N = 2000;

// Physical length of simulation area
const double L = 1.0;

// Wave speed
const double c = 1.0;

// Distance between neighboring cells
const double dx = L / N;

// Time step size. 2D CFL requires dt <= dx/(c*sqrt(2)) ≈ 3.54e-4 at N=2000;
// 2.5e-4 keeps CFL = 0.5 (same safety margin the project used at N=1000).
const double dt = 0.00025;

// Total simulation timesteps
const int STEPS = 2000;

// Save output every 100 steps
const int OUTPUT_FREQ = 100;

// Convert 2D coordinates into 1D array index
inline int idx(int y, int x, int width) {
    return y * width + x;
}

int main(int argc, char* argv[]) {

    // ---------------- MPI INITIALIZATION ----------------

    // Start MPI environment
    MPI_Init(&argc, &argv);

    int rank = 0;   // Process ID
    int size = 1;   // Total number of processes

    // Get this process's rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Get total process count
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // ---------------- DOMAIN DECOMPOSITION ----------------

    // Divide rows equally among processes
    std::vector<int> row_counts(size, N / size);

    // Handle extra rows if N not divisible by size
    for (int i = 0; i < N % size; i++) {
        row_counts[i]++;
    }

    // Compute starting row for each process
    std::vector<int> row_offsets(size, 0);

    for (int i = 1; i < size; i++) {
        row_offsets[i] =
            row_offsets[i - 1] + row_counts[i - 1];
    }

    // Number of rows owned by this process
    const int rows = row_counts[rank];

    // Global starting row index
    const int global_start = row_offsets[rank];

    // Add 2 halo rows:
    // one top halo + one bottom halo
    const int local_rows = rows + 2;

    // ---------------- LOCAL GRID ALLOCATION ----------------

    // Previous timestep
    std::vector<double> h_prev(local_rows * N, 0.0);

    // Current timestep
    std::vector<double> h_curr(local_rows * N, 0.0);

    // Next timestep
    std::vector<double> h_next(local_rows * N, 0.0);

    // ---------------- INITIAL WAVE SETUP ----------------

    const int center_y = N / 2;
    const int center_x = N / 2;

    const double drop_radius = N / 15.0;

    // Initialize only this process's rows
    for (int y = 1; y <= rows; y++) {

        // Convert local row to global row
        const int global_y = global_start + y - 1;

        for (int x = 0; x < N; x++) {

            // Distance from center
            const double distance_squared =
                std::pow(x - center_x, 2) +
                std::pow(global_y - center_y, 2);

            // Create Gaussian wave bump
            if (std::sqrt(distance_squared) < drop_radius * 2) {

                h_curr[idx(y, x, N)] =
                    std::exp(-distance_squared /
                    (drop_radius * drop_radius));

                // Zero initial velocity
                h_prev[idx(y, x, N)] =
                    h_curr[idx(y, x, N)];
            }
        }
    }

    // ---------------- GATHER METADATA ----------------

    // How much data each process sends
    std::vector<int> recv_counts(size);

    // Where received data goes
    std::vector<int> displacements(size);

    for (int i = 0; i < size; i++) {
        recv_counts[i] = row_counts[i] * N;
        displacements[i] = row_offsets[i] * N;
    }

    // Full grid only exists on root process
    std::vector<double> full_grid;

    if (rank == 0) {

        full_grid.resize(N * N);

        std::cout
            << "Starting MPI Tsunami Simulation ("
            << N << "x" << N
            << ") with "
            << size
            << " processes...\n";
    }

    // Synchronize all processes
    MPI_Barrier(MPI_COMM_WORLD);

    // Start timing
    const double start_time = MPI_Wtime();

    // Neighbor process IDs
    const int top_neighbor =
        (rank > 0) ? rank - 1 : MPI_PROC_NULL;

    const int bottom_neighbor =
        (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    // Physics scaling factor
    const double factor =
        (c * c * dt * dt) / (dx * dx);

    // ---------------- UPDATE FUNCTION ----------------

    auto update_row = [&](int y) {

        // Convert local row → global row
        const int global_y = global_start + y - 1;

        // Fixed top/bottom boundaries
        if (global_y == 0 || global_y == N - 1) {

            for (int x = 0; x < N; x++) {
                h_next[idx(y, x, N)] = 0.0;
            }

            return;
        }

        // Left/right fixed boundaries
        h_next[idx(y, 0, N)] = 0.0;
        h_next[idx(y, N - 1, N)] = 0.0;

        // Interior cells
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

    // ---------------- MAIN TIME LOOP ----------------

    for (int t = 0; t < STEPS; t++) {

        // ---------------- HALO EXCHANGE ----------------

        // Non-blocking communication requests
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

        // ---------------- COMPUTE INTERIOR ----------------

        // Compute rows that do NOT depend on halo data
        for (int y = 2; y <= rows - 1; y++) {
            update_row(y);
        }

        // Wait for communication to finish
        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

        // Compute rows touching halo boundaries
        if (rows >= 1) {
            update_row(1);
        }

        if (rows >= 2) {
            update_row(rows);
        }

        // ---------------- ROTATE TIME BUFFERS ----------------

        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);

        // ---------------- SAVE OUTPUT ----------------

        if (t % OUTPUT_FREQ == 0) {

            // Gather all process grids into root process
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

            // Root process writes file
            if (rank == 0) {

                const std::string filename =
                    "../../data/ground_truth/mpi_output_"
                    + std::to_string(t)
                    + ".bin";

                std::ofstream outfile(
                    filename,
                    std::ios::binary
                );

                if (outfile.is_open()) {

                    outfile.write(
                        reinterpret_cast<const char*>(full_grid.data()),
                        full_grid.size() * sizeof(double)
                    );
                }
            }
        }
    }

    // Synchronize before timing end
    MPI_Barrier(MPI_COMM_WORLD);

    const double end_time = MPI_Wtime();

    // Local process runtime
    const double local_time =
        end_time - start_time;

    double max_time = 0.0;

    // Get slowest process time
    MPI_Reduce(
        &local_time,
        &max_time,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );

    // Root process prints timing
    if (rank == 0) {

        std::cout << "Simulation complete.\n";

        std::cout
            << "MPI Execution Time ("
            << size
            << " processes): "
            << max_time
            << " seconds\n";
    }

    // Shut down MPI
    MPI_Finalize();

    return 0;
}