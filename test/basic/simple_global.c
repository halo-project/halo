// RUN: %clang -fhalo -O1 %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo -O1 -fpic -fpie %s -o %t
// RUN: %testhalo %server 1 %t


#define NO_INLINE __attribute__((noinline))

// knobs to control workload
#define ITERS 20

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
