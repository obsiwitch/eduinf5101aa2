#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "mpi.h"
#include "local_matrix2D.h"
#include "array2D.h"

const int BEFORE_TAG = 0;
const int AFTER_TAG = 1;
const int LEFT_TAG = 2;
const int RIGHT_TAG = 3;
const int DISPLAY_TAG = 4;

MPI_Datatype ColType;

// Initialization & update functions

/**
 * Creates and initializes a LocalMatrix.
 */
LocalMatrix createLocalMatrix(int nprocs, int nmatrix, Rank2D rank2D) {
    int innerSize = nmatrix/sqrt(nprocs);
    int totalSize = innerSize + 2;
    
    LocalMatrix localMatrix;
    localMatrix.beforeLine = malloc(innerSize * sizeof(double));
    localMatrix.beforeCol = malloc(innerSize * sizeof(double));
    localMatrix.matrix = malloc2D(innerSize, innerSize);
    localMatrix.afterLine = malloc(innerSize * sizeof(double));
    localMatrix.afterCol = malloc(innerSize * sizeof(double));
    
    localMatrix.innerSize = innerSize;
    localMatrix.totalSize = totalSize;
    
    MPI_Datatype col;
    MPI_Type_vector(
        localMatrix.innerSize, // number of blocks
        1,                     // number of elements per block
        localMatrix.innerSize, // stride
        MPI_DOUBLE, &col
    );
    MPI_Type_commit(&col);
    MPI_Type_create_resized(col, 0, 1*sizeof(double), &ColType);
    MPI_Type_commit(&ColType);
    
    localInitialization(&localMatrix, nprocs, nmatrix, rank2D);
    updateBoundaries(&localMatrix, nprocs, rank2D);
    
    return localMatrix;
}

/**
 * Frees allocated memory for a LocalMatrix.
 */
void destructLocalMatrix(LocalMatrix* matrix) {
    free(matrix->beforeLine);
    free(matrix->beforeCol);
    free2D(matrix->matrix);
    free(matrix->afterLine);
    free(matrix->afterCol);
}

/**
 * Initializes localMatrix.
 */
void localInitialization(
    LocalMatrix* matrix, int nprocs, int nmatrix, Rank2D rank2D
) {
    int lastElement = matrix->innerSize - 1;
    
    // fill the matrix with the same values as in the 1D case to easily check
    // whether the results are correct
    for (int i = 0 ; i < matrix->innerSize ; i++) {
        int value = i / (nmatrix / nprocs);
        value += rank2D.i * sqrt(nprocs);
        fillSeq(matrix->matrix[i], matrix->innerSize, value);
    }
    
    if (rank2D.i == 0) {
        fillSeq(matrix->matrix[0], matrix->innerSize, -1);
        fillSeq(matrix->beforeLine, matrix->innerSize, -2);
    }
    else if (rank2D.i == (int) sqrt(nprocs) - 1) {
        fillSeq(matrix->matrix[lastElement], matrix->innerSize, -1);
        fillSeq(matrix->afterLine, matrix->innerSize, -2);
    }
    
    if (rank2D.j == 0) {
        fillSeq(matrix->beforeCol, matrix->innerSize, -2);
    }
    else if (rank2D.j == (int) sqrt(nprocs) - 1) {
        fillSeq(matrix->afterCol, matrix->innerSize, -2);
    }
}

/**
 * Updates localMatrix's afterLine, beforeLine, afterCol & beforeCol by sending
 * and receiving data from other processes using MPI.
 */
void updateBoundaries(LocalMatrix* matrix, int nprocs, Rank2D rank2D) {
    int nprocs2D = sqrt(nprocs);
    
    // sends
    if (rank2D.i > 0) {
        sendFirstToAfterLine(matrix, nprocs2D, rank2D.rank);
    }
    if (rank2D.i < nprocs2D - 1) {
        sendLastToBeforeLine(matrix, nprocs2D, rank2D.rank);
    }
    if (rank2D.j > 0) {
        sendFirstToAfterCol(matrix, rank2D.rank);
    }
    if (rank2D.j < nprocs2D - 1) {
        sendLastToBeforeCol(matrix, rank2D.rank);
    }
    
    // receives
    if (rank2D.i > 0) {
        recvBeforeFromLastLine(matrix, nprocs2D, rank2D.rank);
    }
    if (rank2D.i < nprocs2D - 1) {
        recvAfterFromFirstLine(matrix, nprocs2D, rank2D.rank);
    }
    if (rank2D.j > 0) {
        recvBeforeFromLastCol(matrix, rank2D.rank);
    }
    if (rank2D.j < nprocs2D - 1) {
        recvAfterFromFirstCol(matrix, rank2D.rank);
    }
}


// Data access functions

/**
 * Checks whether the (i,j) position is a corner.
 */
bool corner(LocalMatrix* matrix, int i, int j) {
    return (i == 0 && j == 0)
        || (i == 0 && j == matrix->totalSize - 1)
        || (i == matrix->totalSize - 1 && j == 0)
        || (i == matrix->totalSize - 1 && j == matrix->totalSize - 1);
}

/**
 * Retrieves the (i,j) element inside the specified matrix.
 */
double get(LocalMatrix* matrix, int i, int j) {
    if (corner(matrix, i, j)) {
        return -4.0;
    }
    else if (i == 0) {
        return matrix->beforeLine[j - 1];
    }
    else if (i == matrix->totalSize - 1) {
        return matrix->afterLine[j - 1];
    }
    else if (j == 0) {
        return matrix->beforeCol[i - 1];
    }
    else if (j == matrix->totalSize - 1) {
        return matrix->afterCol[i - 1];
    }
    else {
        return matrix->matrix[i - 1][j - 1];
    }
}

/**
 * Sets a value to the (i,j) element inside the specified matrix.
 */
void set(LocalMatrix* matrix, int i, int j, double value) {
    if (corner(matrix, i, j)) {
        return;
    }
    else if (i == 0) {
        matrix->beforeLine[j - 1] = value;
    }
    else if (i == matrix->totalSize - 1) {
        matrix->afterLine[j - 1] = value;
    }
    else if (j == 0) {
        matrix->beforeCol[i - 1] = value;
    }
    else if (j == matrix->totalSize - 1) {
        matrix->afterCol[i - 1] = value;
    }
    else {
        matrix->matrix[i - 1][j - 1] = value;
    }
}


// Communication functions

/**
 * Sends the last line from the current process' inner matrix to the beforeLine
 * of the next process
 */
void sendLastToBeforeLine(LocalMatrix* matrix, int nprocs2D, int rank) {
    int lastLine = matrix->innerSize - 1;
    MPI_Send(
        matrix->matrix[lastLine], matrix->innerSize, MPI_DOUBLE,
        downRank(nprocs2D, rank), BEFORE_TAG, MPI_COMM_WORLD
    );
}

/**
 * Sends the first line from the current process' inner matrix to the afterLine
 * of the previous process
 */
void sendFirstToAfterLine(LocalMatrix* matrix, int nprocs2D, int rank) {
    MPI_Send(
        matrix->matrix[0], matrix->innerSize, MPI_DOUBLE,
        upRank(nprocs2D, rank), AFTER_TAG, MPI_COMM_WORLD
    );
}

/**
 * Receives the first line from the next process inner matrix and stores it in
 * the LocalMatrix's afterLine.
 */
void recvAfterFromFirstLine(LocalMatrix* matrix, int nprocs2D, int rank) {
    MPI_Recv(
        matrix->afterLine, matrix->innerSize, MPI_DOUBLE,
        downRank(nprocs2D, rank), AFTER_TAG,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
}

/**
 * Receives the last line from the previous process inner matrix and stores it
 * in the LocalMatrix's beforeLine.
 */
void recvBeforeFromLastLine(LocalMatrix* matrix, int nprocs2D, int rank) {
    MPI_Recv(
        matrix->beforeLine, matrix->innerSize, MPI_DOUBLE,
        upRank(nprocs2D, rank), BEFORE_TAG,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
}

/**
 * Sends the last col from the current process' inner matrix to the beforeCol
 * of the next process
 */
void sendLastToBeforeCol(LocalMatrix* matrix, int rank) {
    int lastCol = matrix->innerSize - 1;
    MPI_Send(
        &matrix->matrix[0][lastCol], 1, ColType,
        rightRank(rank), LEFT_TAG, MPI_COMM_WORLD
    );
}

/**
 * Sends the first col from the current process' inner matrix to the afterCol
 * of the previous process
 */
void sendFirstToAfterCol(LocalMatrix* matrix, int rank) {
    MPI_Send(
        &matrix->matrix[0][0], 1, ColType,
        leftRank(rank), RIGHT_TAG, MPI_COMM_WORLD
    );
}

/**
 * Receives the first col from the next process inner matrix and stores it in
 * the LocalMatrix's afterCol.
 */
void recvAfterFromFirstCol(LocalMatrix* matrix, int rank) {
    MPI_Recv(
        matrix->afterCol, matrix->innerSize, MPI_DOUBLE,
        rightRank(rank), RIGHT_TAG,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
}

/**
 * Receives the last col from the previous process inner matrix and stores it
 * in the LocalMatrix's beforeCol.
 */
void recvBeforeFromLastCol(LocalMatrix* matrix, int rank) {
    MPI_Recv(
        matrix->beforeCol, matrix->innerSize, MPI_DOUBLE,
        leftRank(rank), LEFT_TAG,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE
    );
}


// File writing functions

/**
 * Writes the specified line to a file. Boundaries can be written by setting
 * the associated parameter to true.
 */
void writeMatrixLine(
    LocalMatrix* matrix, char* fileName, int i, bool boundaries, bool endline
) {
    FILE* f = fopen(fileName, "a");
    if (f == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    int jStart = 0;
    int jEnd = matrix->totalSize;
    
    if (!boundaries) { jStart++; jEnd--; }
    
    for (int j = jStart ; j < jEnd ; j++) {
        double value = get(matrix, i, j);
        fprintf(f, "%6.3f ", value);
    }
    
    if (endline) { fprintf(f, "\n"); }
    
    fclose(f);
}

/**
 * Writes the full matrix by alterning between processes to write lines.
 * The processes write in the correct order by using a token ring.
 * Boundaries can be written by setting the associated parameter to true.
 *
 * example:
 * M0 (P0)   M1 (P1)
 * M2 (P2)   M3 (P3)
 *
 * P0 writes first line from M0 and sends a token to P1
 * P1 writes first line from M1 and sends a token to P0
 * ...
 * P0 writes last line from M0 and sends a token to P1
 * P1 writes last line from M1 and sends a token to P2
 * same steps for P2 & P3
 */
void writeFullMatrix(
    LocalMatrix* matrix, int nprocs, Rank2D rank2D, bool boundaries
) {
    bool start = (rank2D.rank == 0);
    if (start) { remove("matrix.out"); }
    
    int nprocsRow = sqrt(nprocs);
    bool lastProcInRow = (rank2D.j == nprocsRow - 1);
    bool lastProc = (rank2D.rank == nprocs - 1);
    
    int nextRank = rightRank(rank2D.rank);
    if (lastProcInRow) { nextRank = nprocsRow * rank2D.i; }
    
    int iStart = 0;
    int iEnd = matrix->totalSize;
    if (!boundaries) { iStart++; iEnd--; }
    
    for (int i = iStart ; i < iEnd ; i++) {
        if (!start) {
            MPI_Recv(
                NULL, 0, MPI_INT, MPI_ANY_SOURCE, DISPLAY_TAG,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE
            );
        } else {
            start = false;
        }
        
        writeMatrixLine(matrix, "matrix.out", i, boundaries, lastProcInRow);
        
        MPI_Send(NULL, 0, MPI_INT, nextRank, DISPLAY_TAG, MPI_COMM_WORLD);
    }
    
    if (lastProcInRow && !lastProc) {
        int rankNextRow = nprocsRow * (rank2D.i + 1);
        MPI_Send(NULL, 0, MPI_INT, rankNextRow, DISPLAY_TAG, MPI_COMM_WORLD);
    }
}
