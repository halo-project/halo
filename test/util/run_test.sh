#!/bin/bash

set -ex

SERVER_EXE=$1
NUM_CLIENTS=$2
PROG_EXE=$3

${SERVER_EXE} --no-persist &
SERVER_PID=$!
sleep 1s

if [ "${NUM_CLIENTS}" -ne "1" ]; then
  echo "Currently only support running one client!"
  exit 1
fi

${PROG_EXE}

wait $SERVER_PID
