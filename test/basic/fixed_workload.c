// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 1 %t
// RUN: %testhalo %server 4 %t

#include "workload.h"

#if defined(SMALL_PROBLEM_SIZE)
  #define ITERS 40
#elif defined(VALGRIND) // we need a long-running client when using valgrind on server.
  #define ITERS 10000
#else
  #define ITERS 160
#endif

int main() {
  return driverFn(ITERS, fixed_getMultiplier, fixed_getLevel);
}