#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <omp.h>

// Simulation Parameters
const int N = 1000;             // Grid Size (N x N)
const double L = 1.0;           // Physical length of the domain
const double c_base = 1.0;      // Base wave speed
const double dx = L / N;        // Spatial step
const double dt = 0.0005;       // Time step (2D CFL: dt <= dx/(c*sqrt(2)))
const int STEPS = 2000;         // Total time steps
const int OUTPUT_FREQ = 100;    // How often to save data to disk

// Inline macro to convert 2D (y, x) coordinates to flattened 1D array index
inline int idx(int y, int x) {
    return y * N + x;
}

int main(int argc, char* argv[]) {
    int num_threads = 1;
    if (argc > 1) {
        num_threads = std::atoi(argv[1]);
    }
    omp_set_num_threads(num_threads);

    // 1. Allocate flattened 1D arrays for the grid
    std::vector<double> h_prev(N * N, 0.0);
    std::vector<double> h_curr(N * N, 0.0);
    std::vector<double> h_next(N * N, 0.0);

    // 2. Initial Condition: Create a Gaussian "drop" in the center to start the wave
    int center_y = N / 2;
    int center_x = N / 2;
    double drop_radius = N / 15.0;
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            double distance_squared = std::pow(x - center_x, 2) + std::pow(y - center_y, 2);
            if (std::sqrt(distance_squared) < drop_radius * 2) {
                h_curr[idx(y, x)] = std::exp(-distance_squared / (drop_radius * drop_radius));
                h_prev[idx(y, x)] = h_curr[idx(y, x)]; // Set initial velocity to 0
            }
        }
    }

    std::cout << "Starting OpenMP Tsunami Simulation (" << N << "x" << N << ") with " << num_threads << " threads...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    // 3. Main Time Loop
    // One persistent parallel region across all time steps — avoids the fork/join
    // cost of re-creating a parallel region every iteration. Inside, `omp for` and
    // `omp single` use implicit barriers to keep threads synchronized.
    const double factor = (c_base * c_base * dt * dt) / (dx * dx);

    #pragma omp parallel
    {
        for (int t = 0; t < STEPS; t++) {

            // Stencil update — split (y, x) iterations across threads
            #pragma omp for collapse(2) schedule(static)
            for (int y = 1; y < N - 1; y++) {
                for (int x = 1; x < N - 1; x++) {

                    // 5-point stencil Laplacian
                    double laplacian = h_curr[idx(y + 1, x)] +
                                       h_curr[idx(y - 1, x)] +
                                       h_curr[idx(y, x + 1)] +
                                       h_curr[idx(y, x - 1)] -
                                       4.0 * h_curr[idx(y, x)];

                    h_next[idx(y, x)] = 2.0 * h_curr[idx(y, x)] - h_prev[idx(y, x)] + factor * laplacian;
                }
            }
            // implicit barrier here — all threads done before boundaries

            // 4. Dirichlet (fixed-wall) Boundary Conditions
            #pragma omp for schedule(static)
            for (int i = 0; i < N; i++) {
                h_next[idx(0, i)] = 0.0;       // Top edge
                h_next[idx(N - 1, i)] = 0.0;   // Bottom edge
                h_next[idx(i, 0)] = 0.0;       // Left edge
                h_next[idx(i, N - 1)] = 0.0;   // Right edge
            }
            // implicit barrier here

            // 5. Pointer swap + optional I/O — one thread only
            #pragma omp single
            {
                std::swap(h_prev, h_curr);
                std::swap(h_curr, h_next);

                if (t % OUTPUT_FREQ == 0) {
                    std::string filename = "../../data/ground_truth/omp_output_" + std::to_string(t) + ".bin";
                    std::ofstream outfile(filename, std::ios::binary);
                    if (outfile.is_open()) {
                        outfile.write(reinterpret_cast<char*>(h_curr.data()), N * N * sizeof(double));
                        outfile.close();
                    }
                }
            }
            // implicit barrier at end of single — all threads see the swapped pointers
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "Simulation complete.\n";
    std::cout << "OpenMP Execution Time (" << num_threads << " threads): " << elapsed.count() << " seconds\n";

    return 0;
}