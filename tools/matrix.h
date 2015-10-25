#ifndef MATRIX
#define MATRIX

struct LocalMatrix {
	double* beforeLine;
	double** matrix;
	double* afterLine;
};

double** malloc2D(int lines, int cols);
void free2D(double** array);

struct LocalMatrix createLocalMatrix(int nprocs, int nmatrix);
double get(struct LocalMatrix* matrix, int nprocs, int nmatrix, int x, int y);
void set(
    struct LocalMatrix* matrix, int nprocs, int nmatrix,
    int x, int y, double value
);
void fillInner(
    struct LocalMatrix* matrix, int nprocs, int nmatrix, double value
);
void sendLastToBeforeLine(
    struct LocalMatrix* matrix, int nprocs, int nmatrix, int currentRank
);
void sendFirstToAfterLine(
    struct LocalMatrix* matrix, int nmatrix, int currentRank
);
void recvAfterFromFirstLine(
    struct LocalMatrix* matrix, int nmatrix, int currentRank
);
void recvBeforeFromLastLine(
    struct LocalMatrix* matrix, int nmatrix, int currentRank
);
void display(struct LocalMatrix* matrix, int nprocs, int nmatrix);

#endif // MATRIX
