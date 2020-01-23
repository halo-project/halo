// RUN: %clang -DNUM_THREADS=1 -fhalo -O1 %s -o %t
// RUN: %testhalo %server 4 %t

// RUN: %clang -DNUM_THREADS=1 -DSMALL_PROBLEM_SIZE -fhalo -O1 -fpic -fpie %s -o %t
// RUN: %testhalo %server 4 %t

#include "linear_hot.h"
