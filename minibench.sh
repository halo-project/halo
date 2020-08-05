#!/bin/bash

# quit if anything fails
set -euo pipefail

TRIALS=5
TUNING_ITERS=30

# limit running time when only doing regression testing
if [[ $# -gt 0 ]]; then
  if [[ $1 == "test" ]]; then
    TRIALS=1
  fi
fi

./test/util/mini_bench.sh ./build $TRIALS $TUNING_ITERS