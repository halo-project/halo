#!/bin/sh

# very simple test of the installation of Halo in the docker
# image _after_ cleaning up all of the build artifacts.
# this effectively just ensures that everything's available.
# We rely on the normal test suite to ensure things actually work correctly.

# NOTE: this script only requires that you have a copy of run_test.sh in the
# same directory, and invoke it from the same working directory.

set -ex

echo "int main () { return 0; }" | clang -fhalo -x c++ -

./run_test.sh haloserver 1 ./a.out

rm a.out
