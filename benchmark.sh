#!/bin/bash

# See https://llvm.org/docs/TestSuiteGuide.html

COMPILER_ROOT=$(pwd)/build
STAMP=$(date +"%m_%d_%Y-%H_%M_%S")

mkdir -p bench-results


# NOTE: should just do a full default build so everything's available in compiler root
# Mainly need llvm-lit, but there doesn't appear to be an install target
# for it. so run `ninja` with no specified target.

for TYPE in O2 O2-halo; do
  rm -rf bench-build
  mkdir bench-build
  pushd bench-build

  cmake -DCMAKE_C_COMPILER="${COMPILER_ROOT}/bin/clang" \
          -C../benchmark/cmake/caches/${TYPE}.cmake \
          -DTEST_SUITE_BENCHMARKING_ONLY=ON \
          -DTEST_SUITE_SUBDIRS="SingleSource" \
          ../benchmark

  make
  "${COMPILER_ROOT}/bin/llvm-lit" -v -j 1 -o "../bench-results/results-${TYPE}-${STAMP}.json" .
  popd
done
