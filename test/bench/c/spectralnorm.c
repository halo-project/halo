// RUN: %clang -O1 -fhalo %s -o %t
// RUN: %testhalo %server 1 %t %t-out.txt
// RUN: diff -w %t-out.txt %s.expected


/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
 *
 * Contributed by Sebastien Loisel
 * Adapted by Kavon Farvardin
 */

#define SPECTRAL_N 2000

#ifdef SMALL_PROBLEM_SIZE
  #define ITERS 16
  #define FLOAT_TY float
#else
  #define ITERS 64
  #define FLOAT_TY double
#endif

 ///////////////////////////
 // benchmark code

#include "stdlib.h"
#include "math.h"
#include "stdio.h"

FLOAT_TY eval_A(int i, int j) { return 1.0/((i+j)*(i+j+1)/2+i+1); }

void eval_A_times_u(int N, const FLOAT_TY u[], FLOAT_TY Au[])
{
  int i,j;
  for(i=0;i<N;i++)
    {
      Au[i]=0;
      for(j=0;j<N;j++) Au[i]+=eval_A(i,j)*u[j];
    }
}

void eval_At_times_u(int N, const FLOAT_TY u[], FLOAT_TY Au[])
{
  int i,j;
  for(i=0;i<N;i++)
    {
      Au[i]=0;
      for(j=0;j<N;j++) Au[i]+=eval_A(j,i)*u[j];
    }
}

void eval_AtA_times_u(int N, const FLOAT_TY u[], FLOAT_TY AtAu[]) {
  FLOAT_TY* v = (FLOAT_TY*) malloc(sizeof(FLOAT_TY) * N);
  eval_A_times_u(N,u,v);
  eval_At_times_u(N,v,AtAu);
  free(v);
}


/////////////////////////////////////////////////
// start of benchmark. default N is 2000
//
// for N = 100, output should be 1.274219991
FLOAT_TY __attribute__((noinline)) spectralnorm (const int N) {
  int i;
  FLOAT_TY* u = (FLOAT_TY*) malloc(sizeof(FLOAT_TY) * N);
  FLOAT_TY* v = (FLOAT_TY*) malloc(sizeof(FLOAT_TY) * N);
  FLOAT_TY vBv,vv;
  for(i=0;i<N;i++) u[i]=1;
  for(i=0;i<10;i++)
    {
      eval_AtA_times_u(N,u,v);
      eval_AtA_times_u(N,v,u);
    }
  vBv=vv=0;
  for(i=0;i<N;i++) { vBv+=u[i]*v[i]; vv+=v[i]*v[i]; }
  free(u);
  free(v);
  return sqrt(vBv/vv);
}

/////////////////////////////////////////////////////////////////

volatile FLOAT_TY ans = 0;

int main() {
  for (unsigned i = 0; i < ITERS; i++) {
    ans = spectralnorm(SPECTRAL_N);
  }

  printf("%f\n", ans);
  return 0;
}