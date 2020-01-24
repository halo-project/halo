// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo -fpic -fpie %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 7 %t

// RUN: %clang -fhalo -fpic -fpie %s -o %t
// RUN: %testhalo %server 7 %t



//////
// when compiled with -O0,
// this should end up wasting time in a function
// that is not patchable, in order to test such
// a situation where there is nothing the server can do.
int main() {
  unsigned i = ~0;

  while (i > 1)
    i--;

  return i-1;
}
