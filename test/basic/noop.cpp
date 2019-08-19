// RUN: %clang -fhalo %s -o %t
// RUN: %testhalo %server 1 %t

// RUN: %clang -fhalo -fPIC -pie %s -o %t
// RUN: %testhalo %server 1 %t

int main() {
  return 0;
}
