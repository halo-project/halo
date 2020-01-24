// RUN: %clang -DNUM_THREADS=4 -fhalo -O1 %s -o %t
// RUN: %testhalo %server 2 %t
// RUN: false

// XFAIL: *

#include "linear_hot.h"
