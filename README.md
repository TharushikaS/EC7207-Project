# EC7207-Project
High-Performance Simulation of Tsunami 

# To run the openmp 
    g++ -fopenmp tsunami_omp.cpp -O3 -o tsunami_openmp


# To run the serial 
    g++ tsunami_serial.cpp -o tsunami_serial


# To run the mpi 
 ---- mpic++ tsunami_mpi.cpp -O3 -o tsunami_mpi
 ---- mpirun -np 1 ./tsunami_mpi
      mpirun -np 2 ./tsunami_mpi
      mpirun -np 4 ./tsunami_mpi
      mpirun -np 8 ./tsunami_mpi -------

