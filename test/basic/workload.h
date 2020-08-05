#pragma once

#include <stdlib.h>
#include <stdio.h>

#define NO_INLINE __attribute__((noinline))

// https://oeis.org/A006577/list
#define START_HAILSTONE 27

volatile long totalSteps;

// a pure, deterministic function whose running time is influenced
// by its inputs. Larger values = more work.
NO_INLINE long workFn(unsigned multiplier, unsigned level) {
  long step_limit = multiplier * level;
  long x = START_HAILSTONE;
  long reachedOne = 0;
  totalSteps = 0;

  while (reachedOne < step_limit) {
    if (x == 1) {
      x = START_HAILSTONE + reachedOne;
      reachedOne++;
    }
    totalSteps++;

    // relies on hailstone sequence to help confuse optimizer.
    if (x % 2 == 0)
      x = x / 2;
    else
      x = 3 * x + 1;
  }

  return totalSteps;
}

unsigned fixed_getMultiplier() {
  return 31250;
}

unsigned fixed_getLevel() {
  return 2;
}

unsigned random_getLevel() {
  return (rand() % 12);
}

int driverFn (int ITERS, unsigned (*getMult)(), unsigned (*getLevel)()) {
  // some junk specially crafted to subvert totally eliminating the computation I want it to perform.
  const unsigned long UNLIKELY_NUMBER = 22193801;
  const unsigned long UNLIKELY_DIVISOR = 5861;

  unsigned long result = 1;
  for (int i = 0; i < ITERS; i++) {
    result += workFn(getMult(), getLevel());
    if (result > UNLIKELY_DIVISOR && (result % UNLIKELY_DIVISOR) == 0)
      i += 2; // NOTE: we do NOT want this to happen!
  }

  if (result == UNLIKELY_NUMBER) {
    // because we somtimes choose random numbers for the level and mult, there's an extremely
    // slim chance that the unlikely number we chose is actually the result of the dummy
    // computation. In that case, we harmlessly print a message as a side-effect that can't
    // be removed.
    fprintf(stderr, "NOT A FAILURE: the unlikely number was actually computed!\n");
  }

  return 0;
}