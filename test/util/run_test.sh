#!/bin/bash

set -e

SERVER_EXE=$1
NUM_CLIENTS=$2
PROG_EXE=$3
PROG_OUT=$4

if [ "$PROG_OUT" == "" ]; then
  PROG_OUT="/dev/null"
fi

${SERVER_EXE} --no-persist --timeout 30 &
SERVER_PID=$!
sleep 1s

if [ "${NUM_CLIENTS}" -ne "1" ]; then
  echo "Currently only support running one client!"
  exit 1
fi

${PROG_EXE} 2>&1 | tee ${PROG_OUT}

wait $SERVER_PID
