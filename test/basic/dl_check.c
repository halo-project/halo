// RUN: %clang %s -ldl -o %t
// RUN: nm %t | grep 'D fib_state'
// RUN: %t

// XFAIL: *

/////////////////
// This simple non-halo test just checks to see if we can access a global
// variable through dlsym.

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>

#define NO_INLINE __attribute__((noinline))

// knobs to control workload
#define ITERS 10

// pick a number such that ITERS * fib(START_FIB) doesn't overflow unsigned long
#define START_FIB 40
#define ANSWER 102334155

unsigned long fib_state = START_FIB;

NO_INLINE unsigned long fib() {

  if (fib_state < 2)
    return fib_state;

  fib_state -= 1;
  unsigned long fst = fib();
  fib_state += 1;

  fib_state -= 2;
  unsigned long snd = fib();
  fib_state += 2;

  return fst + snd;
}

int main()
{

    ///////////////////////////////////////////

    dlerror();
    void* handle = dlopen(NULL, RTLD_NOW);

    if (handle == NULL) {
      char* err = dlerror();
      printf("%s\n", err);
      return 4;
    }

    dlerror();
    void* fib_state_addr = dlsym(handle, "fib_state");

    if (fib_state_addr == NULL) {
      char* err = dlerror();
      printf("%s\n", err);
      return 3;
    }

    ///////////////////////////////////////////


    unsigned long ans = 0;
    int i;

    for (i = 0; i < ITERS; i++) {
      unsigned long res = fib();

      if (res != ANSWER)
        return 2;

      ans += res;
    }

    if (ans % i == 0) {
      return 0;
    }

    return 1;
}
