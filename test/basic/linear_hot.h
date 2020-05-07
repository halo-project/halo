#pragma once

//////
// This test is designed such that it exhibits non-fixed code hotness.
// The first few iterations are only calls to fib, each of which take take
// a constant amount of time.
//
// Then, we begin to run a hailstone sequence calculation, and each call to that
// takes progressively more time (by a linear amount).
//
// Given enough iterations, the hailstone sequence will dominate the running
// time both in aggregate and snapshot.

#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"

#define NO_INLINE __attribute__((noinline))

// knobs to control workload
#define NUM_FIB_ONLY 4

#ifdef SMALL_PROBLEM_SIZE
  #define ITERS 4
#else
  #define ITERS 10
#endif

#ifndef NUM_THREADS
  #define NUM_THREADS 1
#endif

// https://oeis.org/A006577/list
#define START_HAILSTONE 27

// pick a number such that ITERS * fib(START_FIB) doesn't overflow unsigned long
#define START_FIB 40

unsigned long fib_right(unsigned long n);

NO_INLINE unsigned long fib_left(unsigned long n) {
  if (n < 2)
    return n;

  return fib_left(n-1) + fib_right(n-2);
}

NO_INLINE unsigned long fib_right(unsigned long n) {
  if (n < 2)
    return n;

  return fib_left(n-1) + fib_right(n-2);
}

NO_INLINE unsigned long fib(unsigned long n) {
  return fib_left(n);
}

NO_INLINE long compute_hailstone(long limit) {
  long x = START_HAILSTONE;
  long reachedOne = 0;
  long totalSteps = 0;

  while (reachedOne < limit) {
    if (x == 1) {
      x = START_HAILSTONE + reachedOne;
      reachedOne++;
    }
    totalSteps++;

    if (x % 2 == 0)
      x = x / 2;
    else
      x = 3 * x + 1;
  }

  return totalSteps;
}

NO_INLINE void* workFn(void* unused) {
  unsigned long ans = 0;
  unsigned long stoneSteps = 1;
  int i;

  for (i = -NUM_FIB_ONLY; i < ITERS; i++) {
    stoneSteps += compute_hailstone(500000 * i);
    ans += fib(START_FIB);
  }

  if (ans % i == 0) {
    return (void*) 0;
  }

  // return stoneSteps to ensure the computation doesn't get optimized away.
  return (void*) stoneSteps;
}


int main() //(int argc, const char **argv)
{

#if (NUM_THREADS == 1)

  void* retVal = workFn(NULL);
  return retVal != 0;

#else
  pthread_t threads[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&(threads[i]), NULL, workFn, NULL)) {
      perror("thread creation failure");
      exit(2);
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    void* returnStatus;
    if (pthread_join(threads[i], &returnStatus) || returnStatus) {
      perror("thread join failure / non-zero exit code");
      exit(3);
    }
  }
#endif

  return 0;
}