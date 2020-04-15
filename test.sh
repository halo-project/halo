#!/bin/bash

# assumes a build / install setup produced by fresh-build.sh

set -ex

KIND=$1

# TODO: currently check-xray doesn't depend on llvm-xray, but it requires it. this might get fixed upstream.
pushd build
cmake --build . -- llvm-xray check-xray test-halo
popd

##### run benchmark suite
# if [ "${KIND}" == "docker" ]; then
#   git submodule sync benchmark
#   git submodule update --init benchmark
# fi
# ./benchmark.sh

## we could then check the results of the benchmark for perf regressions

if [ "${KIND}" == "docker" ]; then
  pushd test/util
  ./docker-post-install.sh
  popd
fi
