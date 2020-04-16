#!/bin/bash

set -o pipefail

SERVER_EXE=$1
NUM_CLIENTS=$2
PROG_EXE=$3
PROG_OUT=$4
SERV_OUT="$PROG_OUT.out"
SAVING_CLIENT_OUTPUT=1

if [ "$PROG_OUT" == "" ]; then
  PROG_OUT="/dev/null"
  SERV_OUT=$(mktemp)
  SAVING_CLIENT_OUTPUT=0
fi

if [ "${NUM_CLIENTS}" -ne "${NUM_CLIENTS}" ] || [ "${NUM_CLIENTS}" -eq "0" ]; then
  echo "Can't run test with zero clients!"
  exit 1
fi

# NOTE: the timeout is the maximum time (in secs) the server will stay up,
# no matter what! 30min = 1800s
${SERVER_EXE} --no-persist --timeout 1800 > "$SERV_OUT" 2>&1 &
SERVER_PID=$!
sleep 2s

# launch all clients and store PIDs
CLIENT_PIDS=()
# first client's output is the output of this script
${PROG_EXE} 2>&1 | tee ${PROG_OUT} &
PID=$!
CLIENT_PIDS+=($PID)
for ((i = 0 ; i < NUM_CLIENTS-1 ; i++)); do
  # other clients dump output to file
  ${PROG_EXE} > ${PROG_OUT} 2>&1 &
  PID=$!
  CLIENT_PIDS+=($PID)
done

# we can't use set -e because if the client crashes, then we need to wait for
# the server to shutdown before ending this script!
FAILURE=0

# wait for all clients to complete
for PID in "${CLIENT_PIDS[@]}"; do
  if [ $FAILURE -eq "1" ]; then
    kill "$PID"
  else
    wait "$PID" || { echo "A client has failed." && FAILURE=1; }
  fi
done

if [ $FAILURE -eq "1" ]; then
  kill "$SERVER_PID"
else
  # wait for the server to finish
  wait "$SERVER_PID" || { echo "The server has failed." && FAILURE=1; }
fi

# wait for everything else to finish just in case
wait

if [ $FAILURE -eq "1" ]; then
  >&2 echo "Some part of the test has failed! See above."

  >&2 echo -e "\n\n\tSERVER OUTPUT (partial) in file $SERV_OUT"
  >&2 tail -n 50 "$SERV_OUT"

  if [ $SAVING_CLIENT_OUTPUT -eq "1" ]; then
    >&2 echo -e "\n\n\tONE CLIENT'S OUTPUT (partial) from file $PROG_OUT"
    >&2 tail -n 50 "$PROG_OUT"
  fi
  exit 1
else
  rm "$SERV_OUT"   # delete the temporary file we generated
fi
