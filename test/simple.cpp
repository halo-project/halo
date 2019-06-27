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

#define NO_INLINE __attribute__((noinline))

// knobs to control workload
#define NUM_FIB_ONLY 4
#define ITERS 10

// https://oeis.org/A006577/list
#define START_HAILSTONE 27

// pick a number such that ITERS * fib(START_FIB) doesn't overflow unsigned long
#define START_FIB 40

NO_INLINE unsigned long fib(unsigned long n) {
  if (n < 2)
    return n;

  return fib(n-1) + fib(n-2);
}

NO_INLINE void compute_hailstone(long limit) {
  long x = START_HAILSTONE;
  long reachedOne = 0;

  while (reachedOne < limit) {
    if (x == 1) {
      x = START_HAILSTONE;
      reachedOne++;
    }

    if (x % 2 == 0)
      x = x / 2;
    else
      x = 3 * x + 1;
  }
}

int main() //(int argc, const char **argv)
{
    unsigned long ans = 0;
    int i;

    for (i = -NUM_FIB_ONLY; i < ITERS; i++) {
      compute_hailstone(500'000 * i);
      ans += fib(START_FIB);
    }

    if (ans % i == 0) {
      return 0;
    }

    return 1;
}
