// RUN: %clang -DSMALL_PROBLEM_SIZE -fhalo %s -o %t
// RUN: %testhalo %server 1 %t
// RUN: %testhalo %server 4 %t

#include "workload.h"

#ifdef SMALL_PROBLEM_SIZE
  #define ITERS 160
#else
  #define ITERS 320
#endif

int main() {
  return driverFn(ITERS, fixed_getMultiplier, fixed_getLevel);
}