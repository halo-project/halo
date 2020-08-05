// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 1 %t
// RUN: %testhalo %server 4 %t

#include "workload.h"
#include <time.h>

#ifdef SMALL_PROBLEM_SIZE
  #define ITERS 40
#else
  #define ITERS 160
#endif

int main() {
  srand(time(0));
  return driverFn(ITERS, fixed_getMultiplier, random_getLevel);
}