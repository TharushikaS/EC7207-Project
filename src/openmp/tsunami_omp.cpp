#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <omp.h>

// --- Benchmark Parameters ---
// const int N = 2000;
const int N = 500;             // Increased to 2000x2000 to stress the CPU
const double L = 1.0;           
const double c = 1.0;           
const double dx = L / N;        
const double dt = 0.0005;       // Smaller dt needed for larger N
// const int STEPS = 1000;         
const int STEPS = 2000;         // Reduced steps since each step is 16x heavier

inline int idx(int y, int x) {
    return y * N + x;
}

int main(int argc, char* argv[]) {
    int num_threads = 1;
    if (argc > 1) {
        num_threads = std::atoi(argv[1]);
    }
    omp_set_num_threads(num_threads);

    std::vector<double> h_prev(N * N, 0.0);
    std::vector<double> h_curr(N * N, 0.0);
    std::vector<double> h_next(N * N, 0.0);

    int center_y = N / 2;
    int center_x = N / 2;
    double drop_radius = N / 15.0;

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            double dist_sq = std::pow(x - center_x, 2) + std::pow(y - center_y, 2);
            if (std::sqrt(dist_sq) < drop_radius * 2) {
                h_curr[idx(y, x)] = std::exp(-dist_sq / (drop_radius * drop_radius));
                h_prev[idx(y, x)] = h_curr[idx(y, x)]; 
            }
        }
    }
    
    // Start timing ONLY the pure math
    double start_time = omp_get_wtime();

    for (int t = 0; t < STEPS; t++) {
        
        // Parallelize only the outer row to maximize memory cache hits
        #pragma omp parallel for schedule(static)
        for (int y = 1; y < N - 1; y++) {
            for (int x = 1; x < N - 1; x++) {
                double laplacian = h_curr[idx(y + 1, x)] + 
                                   h_curr[idx(y - 1, x)] + 
                                   h_curr[idx(y, x + 1)] + 
                                   h_curr[idx(y, x - 1)] - 
                                   4.0 * h_curr[idx(y, x)];

                double factor = (c * c * dt * dt) / (dx * dx);
                h_next[idx(y, x)] = 2.0 * h_curr[idx(y, x)] - h_prev[idx(y, x)] + factor * laplacian;
            }
        }

        #pragma omp parallel for schedule(static)
        for (int i = 0; i < N; i++) {
            h_next[idx(0, i)] = 0.0;       
            h_next[idx(N - 1, i)] = 0.0;   
            h_next[idx(i, 0)] = 0.0;       
            h_next[idx(i, N - 1)] = 0.0;   
        }

        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);
    }

    double end_time = omp_get_wtime();
    std::cout << "Threads: " << num_threads << " | Time: " << (end_time - start_time) << "s\n";

    return 0;
}