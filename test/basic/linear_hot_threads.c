// RUN: %clang -DNUM_THREADS=4 -fhalo -O1 %s -o %t
// RUN: %testhalo %server 1 %t

#include "linear_hot.h"
