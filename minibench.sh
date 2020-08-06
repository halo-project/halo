#!/bin/bash

# quit if anything fails
set -eo pipefail

SELF_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [[ ! -f "$SELF_DIR/minibench.sh" ]]; then
  echo "must run minibench.sh from the same directory in which the file resides."
  exit 1
fi

TRIALS=5
TUNING_ITERS=30

# limit running time when only doing regression testing
if [[ $# -gt 0 ]]; then
  if [[ $1 == "test" ]]; then
    TRIALS=1
  fi
fi

# if USE_ARMENTA is set, then that's the ip address of this local host
# and then we run the client on armenta.
#
# otherwise we set it to 0 to indicate local client.
if [[ -z $USE_ARMENTA ]]; then
  export USE_ARMENTA=0
fi

./test/util/mini_bench.sh build $TRIALS $TUNING_ITERS