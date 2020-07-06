#pragma once

// "restrict" is not a C++ keyword/type qualifier,
// but compilers recognize the hint in c++
#if defined(__GNUC__) || defined(__clang__)
  #define NOALIAS __restrict__
#else
  #error "add your compiler here"
#endif

// indexing into flattened, dense 2D matrix
#define IDX(Mat, ColWidth, Row, Col) \
    Mat[(Row) * (ColWidth) + (Col)]

// computes  C = AB  where
// C is an MxN dense matrix
// A is an MxK dense matrix
// B is an KxN dense matrix
// and all matrices are non-overlapping in memory.
template<typename ElmTy>
__attribute__((noinline))
void matmul(int M, int N, int K, ElmTy * NOALIAS C, ElmTy * NOALIAS A, ElmTy * NOALIAS B) {
  for (int m = 0; m < M; m += 1)
    for (int n = 0; n < N; n += 1)
      for (int k = 0; k < K; k += 1)
        IDX(C, N, m, n) += IDX(A, K, m, k) * IDX(B, N, k, n);
}