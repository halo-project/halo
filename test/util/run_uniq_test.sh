#!/bin/bash

set -o pipefail

SERVER_EXE=$1
SERV_OUT="$SERVER_EXE.out"

if [[ $# -lt 2 ]]; then
  echo "must provide client executables after server exe"
  exit 1
fi

# enable core dumps
ulimit -c unlimited
# on Ubuntu, you probably need to change the core path so it gets dumped to a file before getting piped to apport!
# you can do so with this:
#
#  sudo sysctl -w kernel.core_pattern="| /usr/bin/tee /tmp/core-%e.%p.%h.%t | /usr/share/apport/apport %p %s %c %d %P %E"
#

# NOTE: the timeout is the maximum time (in secs) the server will stay up,
# no matter what! 30min = 1800s
${SERVER_EXE} --halo-threads=3 --halo-no-persist --halo-timeout 1800 > "$SERV_OUT" 2>&1 &
SERVER_PID=$!
sleep 2s


# launch all clients and store PIDs. redirect their output to different files.
CLIENT_PIDS=()
for CLIENT_EXE in "${@:2}" ; do
  ${CLIENT_EXE} > "${CLIENT_EXE}.out" 2>&1 &
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
    wait "$PID" || { echo "A client (pid = $PID) has failed." && FAILURE=1; }
  fi
done

if [ $FAILURE -eq "1" ]; then
  kill "$SERVER_PID"
else
  # wait for the server to finish
  wait "$SERVER_PID" || { echo "The server (pid = $SERVER_PID) has failed." && FAILURE=1; }
fi

# wait for everything else to finish just in case
wait

if [ $FAILURE -eq "1" ]; then
  echo "Some part of the test has failed! See above."
  echo -e "\n\n\tSERVER OUTPUT:"
  tail -n 100 "$SERV_OUT"

  for CLIENT_EXE in "${@:2}" ; do
    echo -e "\n\n\tCLIENT OUTPUT:"
    tail -n 10 "${CLIENT_EXE}.out"
  done

  exit 1
fi
