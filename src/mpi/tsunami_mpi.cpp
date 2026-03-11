#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

const int N = 500;
const double L = 1.0;
const double c = 1.0;
const double dx = L / N;
const double dt = 0.001;
const int STEPS = 2000;

inline int idx(int y, int x, int width)
{
    return y * width + x;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // no of rows handled by each process
    int rows = N / size;

    // Extra halo rows for neighbor communication
    int local_rows = rows + 2;

    vector<double> h_prev(local_rows * N, 0.0);
    vector<double> h_curr(local_rows * N, 0.0);
    vector<double> h_next(local_rows * N, 0.0);

    // determine starting global row
    int global_start = rank * rows;

    int center = N / 2;
    double radius = N / 15.0;

    // initial Gaussian disturbance

    for (int y = 1; y <= rows; y++)
    {
        int global_y = global_start + y - 1;

        for (int x = 0; x < N; x++)
        {
            double dist =
                (x - center) * (x - center) +
                (global_y - center) * (global_y - center);

            if (sqrt(dist) < radius * 2)
            {
                h_curr[idx(y, x, N)] =
                    exp(-dist / (radius * radius));

                h_prev[idx(y, x, N)] =
                    h_curr[idx(y, x, N)];
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();

    for (int t = 0; t < STEPS; t++)
    {

        if (rank > 0)
        {
            MPI_Send(&h_curr[idx(1, 0, N)], N,
                     MPI_DOUBLE, rank - 1, 0,
                     MPI_COMM_WORLD);

            MPI_Recv(&h_curr[idx(0, 0, N)], N,
                     MPI_DOUBLE, rank - 1, 0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
        }

        if (rank < size - 1)
        {
            MPI_Send(&h_curr[idx(rows, 0, N)], N,
                     MPI_DOUBLE, rank + 1, 0,
                     MPI_COMM_WORLD);

            MPI_Recv(&h_curr[idx(rows + 1, 0, N)], N,
                     MPI_DOUBLE, rank + 1, 0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
        }

        // Update local grid
        for (int y = 1; y <= rows; y++)
        {
            for (int x = 1; x < N - 1; x++)
            {
                double lap =
                    h_curr[idx(y + 1, x, N)] +
                    h_curr[idx(y - 1, x, N)] +
                    h_curr[idx(y, x + 1, N)] +
                    h_curr[idx(y, x - 1, N)] -
                    4 * h_curr[idx(y, x, N)];

                double factor =
                    (c * c * dt * dt) / (dx * dx);

                h_next[idx(y, x, N)] =
                    2 * h_curr[idx(y, x, N)] -
                    h_prev[idx(y, x, N)] +
                    factor * lap;
            }
        }

        // Rotate buffers
        swap(h_prev, h_curr);
        swap(h_curr, h_next);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double end = MPI_Wtime();

    double local_time = end - start;

    // Print time for each process in order
    for (int i = 0; i < size; i++)
    {
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == i)
        {
            cout << "Process " << rank << " Time: " << local_time << " seconds" << endl;
        }
    }

    MPI_Finalize();

    return 0;
}

// #include <mpi.h>
// #include <iostream>
// #include <vector>
// #include <cmath>
// #include <algorithm>

// const int N = 500;
// const double L = 1.0;
// const double c = 1.0;
// const double dx = L / N;
// const double dt = 0.001;
// const int STEPS = 2000;

// inline int idx(int y, int x, int width)
// {
//     return y * width + x;
// }

// int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rows = N / size;

    int local_rows = rows + 2; // halo rows

    std::vector<double> h_prev(local_rows * N, 0.0);
    std::vector<double> h_curr(local_rows * N, 0.0);
    std::vector<double> h_next(local_rows * N, 0.0);

    int global_start = rank * rows;

    int center = N / 2;
    double radius = N / 15.0;

    for (int y = 1; y <= rows; y++)
    {
        int global_y = global_start + y - 1;

        for (int x = 0; x < N; x++)
        {
            double dist = (x - center) * (x - center) + (global_y - center) * (global_y - center);

            if (sqrt(dist) < radius * 2)
            {
                h_curr[idx(y, x, N)] = exp(-dist / (radius * radius));
                h_prev[idx(y, x, N)] = h_curr[idx(y, x, N)];
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double start = MPI_Wtime();

    for (int t = 0; t < STEPS; t++)
    {

        if (rank > 0)
        {
            MPI_Send(&h_curr[idx(1, 0, N)], N, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD);
            MPI_Recv(&h_curr[idx(0, 0, N)], N, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        if (rank < size - 1)
        {
            MPI_Send(&h_curr[idx(rows, 0, N)], N, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD);
            MPI_Recv(&h_curr[idx(rows + 1, 0, N)], N, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        for (int y = 1; y <= rows; y++)
        {
            for (int x = 1; x < N - 1; x++)
            {
                double lap =
                    h_curr[idx(y + 1, x, N)] +
                    h_curr[idx(y - 1, x, N)] +
                    h_curr[idx(y, x + 1, N)] +
                    h_curr[idx(y, x - 1, N)] -
                    4 * h_curr[idx(y, x, N)];

                double factor = (c * c * dt * dt) / (dx * dx);

                h_next[idx(y, x, N)] =
                    2 * h_curr[idx(y, x, N)] -
                    h_prev[idx(y, x, N)] +
                    factor * lap;
            }
        }

        std::swap(h_prev, h_curr);
        std::swap(h_curr, h_next);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double end = MPI_Wtime();

    if (rank == 0)
    {
        std::cout << "Process " << rank
                  << " Time: " << (end - start)
                  << " seconds\n";
    }

    MPI_Finalize();
}