// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo -fpic -fpie %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 50 %t

// RUN: %clang -fhalo -fpic -fpie %s -o %t
// RUN: %testhalo %server 50 %t

int main() {
  return 0;
}
