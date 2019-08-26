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

NO_INLINE unsigned long fib() {
  static unsigned long n = START_FIB;

  if (n < 2)
    return n;

  n -= 1;
  unsigned long fst = fib();
  n += 1;

  n -= 2;
  unsigned long snd = fib();
  n += 2;

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
