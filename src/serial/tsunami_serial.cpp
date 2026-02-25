#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>

// --- Simulation Parameters ---
const int N = 500;              // Grid Size (N x N)
const double L = 1.0;           // Physical length of the domain
const double c = 1.0;           // Wave speed
const double dx = L / N;        // Spatial step
const double dt = 0.001;        // Time step (Must satisfy dt < dx/c for stability)
const int STEPS = 2000;         // Total time steps
const int OUTPUT_FREQ = 100;    // How often to save data to disk

// Inline macro to convert 2D (y, x) coordinates to flattened 1D array index 
inline int idx(int y, int x) {
    return y * N + x;
}

int main() {
    // 1. Allocate flattened 1D arrays for the grid 
    std::vector<double> h_prev(N * N, 0.0);
    std::vector<double> h_curr(N * N, 0.0);
    std::vector<double> h_next(N * N, 0.0);

    // 2. Initial Condition: Create a Gaussian "drop" in the center to start the wave
    int center_y = N / 2;
    int center_x = N / 2;
    double drop_radius = N / 15.0;

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            double distance_squared = std::pow(x - center_x, 2) + std::pow(y - center_y, 2);
            if (std::sqrt(distance_squared) < drop_radius * 2) {
                h_curr[idx(y, x)] = std::exp(-distance_squared / (drop_radius * drop_radius));
                h_prev[idx(y, x)] = h_curr[idx(y, x)]; // Set initial velocity to 0
            }
        }
    }

    std::cout << "Starting Serial Tsunami Simulation (" << N << "x" << N << ")...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    // 3. Main Time Loop
    for (int t = 0; t < STEPS; t++) {
        
        // Double nested loop for grid updates 
        // Bounding from 1 to N-1 to leave the edges for boundary conditions
        for (int y = 1; y < N - 1; y++) {
            for (int x = 1; x < N - 1; x++) {
                
                // 5-point stencil Laplacian
                double laplacian = h_curr[idx(y + 1, x)] + 
                                   h_curr[idx(y - 1, x)] + 
                                   h_curr[idx(y, x + 1)] + 
                                   h_curr[idx(y, x - 1)] - 
                                   4.0 * h_curr[idx(y, x)];

                // Update water height using the wave equation
                double factor = (c * c * dt * dt) / (dx * dx);
                h_next[idx(y, x)] = 2.0 * h_curr[idx(y, x)] - h_prev[idx(y, x)] + factor * laplacian;
            }
        }

        // 4. Reflective Boundary Conditions 
        // Keeping the edges at exactly 0.0 acts as a hard wall, reflecting the wave back.
        for (int i = 0; i < N; i++) {
            h_next[idx(0, i)] = 0.0;       // Top edge
            h_next[idx(N - 1, i)] = 0.0;   // Bottom edge
            h_next[idx(i, 0)] = 0.0;       // Left edge
            h_next[idx(i, N - 1)] = 0.0;   // Right edge
        }

        // 5. Pointer Swap (Optimized memory rotation)
        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);

        // 6. Save ground truth data for accuracy validation 
        if (t % OUTPUT_FREQ == 0) {
            // Note: Ensure the 'data/ground_truth' directory exists before running
            std::string filename = "../../data/ground_truth/output_" + std::to_string(t) + ".bin";
            std::ofstream outfile(filename, std::ios::binary);
            if (outfile.is_open()) {
                outfile.write(reinterpret_cast<char*>(h_curr.data()), N * N * sizeof(double));
                outfile.close();
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "Simulation complete.\n";
    std::cout << "Serial Execution Time: " << elapsed.count() << " seconds\n";

    return 0;
}