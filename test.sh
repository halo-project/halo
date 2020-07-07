#!/bin/bash

# assumes a build / install setup produced by fresh-build.sh

set -ex

# the words "docker" and "full" are recognized in this string,
# i.e, docker-full vs docker vs full vs ""
KIND=$1

pushd build

if [[ ${KIND} =~ .*full.* ]]; then
  # make sure relevant parts of LLVM are okay
  cmake --build . -- check-llvm-transforms
  cmake --build . -- check-llvm-codegen-x86
  cmake --build . -- check-clang
fi

cmake --build . -- llvm-xray
cmake --build . -- check-xray

####
# FIXME: this fails on most tests with lli saying
#     lli: error: error creating EE: Unable to find target for this triple (no targets are registered)
# cmake --build . -- check-llvm-executionengine

# test halo
cmake --build . -- test-halo

popd

##### run benchmark suite
# if [ "${KIND}" == "docker" ]; then
#   git submodule sync benchmark
#   git submodule update --init benchmark
# fi
# ./benchmark.sh

## we could then check the results of the benchmark for perf regressions

if [[ ${KIND} =~ .*docker.* ]]; then
  pushd test/util
  ./docker-post-install.sh
  popd
fi
