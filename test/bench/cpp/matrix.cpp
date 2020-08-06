// RUN: %clang -O1 -fhalo %s -o %t
// RUN: %testhalo %server 1 %t %t-out.txt

#include "matmul.hpp"

#include <vector>
#include <numeric>

// initializes some parts of the matrix with known values
template<typename ElmTy>
void verifyInit(int M, int N, int K, ElmTy *A, ElmTy *B) {
  const int ROW = 1;
  const int COL = 4;

  // clear a row of A
  for (int i = 0; i < M; i++)
    IDX(A, K, ROW, i) = 0;

  // set two elms of that row
  IDX(A, K, ROW, 2) = 2;
  IDX(A, K, ROW, 3) = 3;


  // clear a column of B
  for (int i = 0; i < N; i++)
    IDX(B, N, i, COL) = 0;

  // set two elms of that col
  IDX(B, N, 2, COL) = 5;
  IDX(B, N, 3, COL) = 7;
}

// checks the result of part of the matrix, as setup by verifyInit
// returns true if it is correct.
template<typename ElmTy>
bool verifyCheck(int N, ElmTy *C) {
  // C[1][4] = A[1][2] * B[2][4] + A[1][3] * B[3][4]
  //         =    2    *    5    +    3    *    7
  //         =         10        +        21
  //         =                  31
  return IDX(C, N, 1, 4) == 31;
}


#define DIM 256

#ifdef SMALL_PROBLEM_SIZE
  #define ITERS 125
  using ElmTy = float;
#else
  #define ITERS 500
  using ElmTy = double;
#endif

int main() {
  const int M = DIM;
  const int N = DIM;
  const int K = DIM;

  // alloc
  std::vector<ElmTy> A(M*K);
  std::vector<ElmTy> B(K*N);
  std::vector<ElmTy> C(M*N);

  // init with some non-zero junk
  std::iota(A.begin(), A.end(), 2);
  std::iota(B.begin(), B.end(), 5);

  // init part of the mats with known vals
  verifyInit<ElmTy>(M, N, K, A.data(), B.data());

  for (int i = 0; i < ITERS; i++) {
    // clear C
    std::fill(C.begin(), C.end(), 0);

    // multiply
    matmul<ElmTy>(M, N, K, C.data(), A.data(), B.data());

    // check
    if (!verifyCheck<ElmTy>(N, C.data()))
      return 1;
  }

  return 0;
}