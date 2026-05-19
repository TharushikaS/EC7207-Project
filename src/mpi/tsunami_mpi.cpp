#include <mpi.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const int N = 500;              // Grid Size (N x N)
const double L = 1.0;           // Physical length of the domain
const double c = 1.0;           // Wave speed
const double dx = L / N;        // Spatial step
const double dt = 0.001;        // Time step
const int STEPS = 2000;         // Total time steps
const int OUTPUT_FREQ = 100;    // How often to save data to disk

inline int idx(int y, int x, int width) {
    return y * width + x;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::vector<int> row_counts(size, N / size);
    for (int i = 0; i < N % size; i++) {
        row_counts[i]++;
    }

    std::vector<int> row_offsets(size, 0);
    for (int i = 1; i < size; i++) {
        row_offsets[i] = row_offsets[i - 1] + row_counts[i - 1];
    }

    const int rows = row_counts[rank];
    const int global_start = row_offsets[rank];
    const int local_rows = rows + 2; // Includes top and bottom halo rows.

    std::vector<double> h_prev(local_rows * N, 0.0);
    std::vector<double> h_curr(local_rows * N, 0.0);
    std::vector<double> h_next(local_rows * N, 0.0);

    const int center_y = N / 2;
    const int center_x = N / 2;
    const double drop_radius = N / 15.0;

    for (int y = 1; y <= rows; y++) {
        const int global_y = global_start + y - 1;

        for (int x = 0; x < N; x++) {
            const double distance_squared =
                std::pow(x - center_x, 2) + std::pow(global_y - center_y, 2);

            if (std::sqrt(distance_squared) < drop_radius * 2) {
                h_curr[idx(y, x, N)] =
                    std::exp(-distance_squared / (drop_radius * drop_radius));
                h_prev[idx(y, x, N)] = h_curr[idx(y, x, N)];
            }
        }
    }

    std::vector<int> recv_counts(size);
    std::vector<int> displacements(size);
    for (int i = 0; i < size; i++) {
        recv_counts[i] = row_counts[i] * N;
        displacements[i] = row_offsets[i] * N;
    }

    std::vector<double> full_grid;
    if (rank == 0) {
        full_grid.resize(N * N);
        std::cout << "Starting MPI Tsunami Simulation (" << N << "x" << N
                  << ") with " << size << " processes...\n";
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double start_time = MPI_Wtime();

    const int top_neighbor = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
    const int bottom_neighbor = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;
    const double factor = (c * c * dt * dt) / (dx * dx);

    auto update_row = [&](int y) {
        const int global_y = global_start + y - 1;

        if (global_y == 0 || global_y == N - 1) {
            for (int x = 0; x < N; x++) {
                h_next[idx(y, x, N)] = 0.0;
            }
            return;
        }

        h_next[idx(y, 0, N)] = 0.0;
        h_next[idx(y, N - 1, N)] = 0.0;

        for (int x = 1; x < N - 1; x++) {
            const double laplacian =
                h_curr[idx(y + 1, x, N)] +
                h_curr[idx(y - 1, x, N)] +
                h_curr[idx(y, x + 1, N)] +
                h_curr[idx(y, x - 1, N)] -
                4.0 * h_curr[idx(y, x, N)];

            h_next[idx(y, x, N)] =
                2.0 * h_curr[idx(y, x, N)] - h_prev[idx(y, x, N)] +
                factor * laplacian;
        }
    };

    for (int t = 0; t < STEPS; t++) {
        MPI_Request requests[4];
        MPI_Irecv(&h_curr[idx(0, 0, N)], N, MPI_DOUBLE, top_neighbor, 1,
                  MPI_COMM_WORLD, &requests[0]);
        MPI_Irecv(&h_curr[idx(rows + 1, 0, N)], N, MPI_DOUBLE, bottom_neighbor, 0,
                  MPI_COMM_WORLD, &requests[1]);
        MPI_Isend(&h_curr[idx(1, 0, N)], N, MPI_DOUBLE, top_neighbor, 0,
                  MPI_COMM_WORLD, &requests[2]);
        MPI_Isend(&h_curr[idx(rows, 0, N)], N, MPI_DOUBLE, bottom_neighbor, 1,
                  MPI_COMM_WORLD, &requests[3]);

        for (int y = 2; y <= rows - 1; y++) {
            update_row(y);
        }

        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

        if (rows >= 1) {
            update_row(1);
        }
        if (rows >= 2) {
            update_row(rows);
        }

        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);

        if (t % OUTPUT_FREQ == 0) {
            MPI_Gatherv(&h_curr[idx(1, 0, N)], rows * N, MPI_DOUBLE,
                        full_grid.data(), recv_counts.data(), displacements.data(),
                        MPI_DOUBLE, 0, MPI_COMM_WORLD);

            if (rank == 0) {
                const std::string filename =
                    "../../data/ground_truth/mpi_output_" + std::to_string(t) + ".bin";
                std::ofstream outfile(filename, std::ios::binary);
                if (outfile.is_open()) {
                    outfile.write(reinterpret_cast<const char*>(full_grid.data()),
                                  full_grid.size() * sizeof(double));
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double end_time = MPI_Wtime();
    const double local_time = end_time - start_time;

    double max_time = 0.0;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Simulation complete.\n";
        std::cout << "MPI Execution Time (" << size
                  << " processes): " << max_time << " seconds\n";
    }

    MPI_Finalize();
    return 0;
}
