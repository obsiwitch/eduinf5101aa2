#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "mpi.h"
#include "tools/local_matrix2D.h"
#include "tools/rank2D.h"

const int N_MATRIX = 12;

int main(int argc, char** argv) {
    int nprocs, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    
    Rank2D rank2D = createRank2D(nprocs, rank);
    LocalMatrix localMatrix = createLocalMatrix(nprocs, N_MATRIX, rank2D);

    MPI_Barrier(MPI_COMM_WORLD);

    writeFullMatrix(&localMatrix, nprocs, rank2D, false);

    destructLocalMatrix(&localMatrix);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
