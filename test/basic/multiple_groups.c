// RUN: %clang -fhalo -DCLIENT_NUM=1 %s -o %t_1
// RUN: %clang -fhalo -DCLIENT_NUM=2 %s -o %t_2
// RUN: %clang -fhalo -DCLIENT_NUM=3 %s -o %t_3
// RUN: %clang -fhalo -DCLIENT_NUM=4 %s -o %t_4

// RUN: %testuniqhalo %server %t_1 %t_2 %t_3 %t_4

// RUN: FileCheck -check-prefix=CHECK-V1 < %t_1.out %s
// RUN: FileCheck -check-prefix=CHECK-V2 < %t_2.out %s
// RUN: FileCheck -check-prefix=CHECK-V3 < %t_3.out %s
// RUN: FileCheck -check-prefix=CHECK-V4 < %t_4.out %s

// CHECK-V1: ans = 18087721376940707900
// CHECK-V2: ans = 153890044419755474
// CHECK-V3: ans = 14595375066858776825
// CHECK-V4: ans = 8427354705816047154

// In this test, we launch 1 instance of the server
// and have multiple __different__ clients connect.

#include <stdint.h>
#include <stdio.h>

#define NO_INLINE __attribute__((noinline))

#define LOOPS 200000

// if you change this, you'll need to recompute the final values.
#define RANDOM_STEPS 10000

#ifndef CLIENT_NUM
  #error "Must define a client number!"
#endif

// NOT a correct implementation, just want to detect wrong bitcode sent by server.
NO_INLINE uint64_t xorshift(uint64_t x) {
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return x + CLIENT_NUM;
}

volatile uint64_t ans = CLIENT_NUM;

int main()
{

  for (int i = 0; i < LOOPS; i++) {
    ans = CLIENT_NUM;

    for (int k = 0; k < RANDOM_STEPS; k++)
      ans = xorshift(ans);

    if (CLIENT_NUM == 1 && ans != 18087721376940707900ULL)
      return 1;

    if (CLIENT_NUM == 2 && ans != 153890044419755474ULL)
      return 1;

    if (CLIENT_NUM == 3 && ans != 14595375066858776825ULL)
      return 1;

    if (CLIENT_NUM == 4 && ans != 8427354705816047154ULL)
      return 1;

  }

  printf("ans = %llu\n", (unsigned long long) ans);
  return 0;
}
