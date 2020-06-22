#!/bin/bash

# quit if anything fails
set -euo pipefail

ROOT=$1
NUM_TRIALS=$2   # number of fresh trials, to average the results.
NUM_ITERS=$3    # number of times to run the program _per_ trial

STAMP=$(date +"%m_%d_%Y-%H_%M_%S")
CSVFILE="./minibench-${STAMP}.csv"

if [[ $# -lt 2 ]]; then
  echo "must provide <path to root of HALO build / install dir> <num-trials>"
  exit 1
fi

if [[ ! $NUM_TRIALS =~ [0-9]+ ]]; then
  echo "num trials = ${NUM_TRIALS} doesn't make sense."
  exit 1
fi

if [[ ! $NUM_ITERS =~ [0-9]+ ]]; then
  echo "num iters = ${NUM_ITERS} doesn't make sense."
  exit 1
fi

SERVER_EXE="$ROOT/bin/haloserver"
CLANG_EXE="$ROOT/bin/clang"
TIME_EXE=$(command -v time)  # do not want built-in bash 'time'

if [ -z "$TIME_EXE" ]; then
  echo "GNU time command is missing!"
  exit 1
fi

# the superset of all libs possibly needed by all benchmarks.
LIBS="-lm"

# control problem-size, etc.
PREPROCESSOR_FLAGS="-DSMALL_PROBLEM_SIZE"

# get directory of this script
SELF_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
TEST_DIR="$SELF_DIR/.."

declare -a BENCHMARKS=(
  "basic/fixed_workload.c"
  "basic/random_workload.c"
  "bench/c/almabench.c"
)

declare -a OPTIONS=(
  "-O0"
  "withserver -fhalo -O0"
  "-fhalo -O0"
  "-O1"
  "withserver -fhalo -O1"
  "-fhalo -O1"
  "-O2"
  "withserver -fhalo -O2"
  "-fhalo -O2"
  "-O3"
  "withserver -fhalo -O3"
  "-fhalo -O3"
)

# overwrite and create the file

HEADER="program,flags,trial,iter,time"
echo "${HEADER}" | tee "${CSVFILE}"

# we need to send output of the time command to a separate file.
TIME_OUTPUT_FILE=$(mktemp)

SERVER_PID="" # clear it to make sure we don't kill something random!
# kill the current server PID if we ever get a ctrl+c
trap 'kill -9 $SERVER_PID' SIGINT

for PROG in "${BENCHMARKS[@]}"; do
  for FLAGS in "${OPTIONS[@]}"; do
    COMPILE_FLAGS=${FLAGS//withserver/}  # remove withserver from flags
    # NOTE: do NOT double-quote COMPILE_FLAGS!
    CLIENT_BIN="./a.out"
    ${CLANG_EXE} ${PREPROCESSOR_FLAGS} ${COMPILE_FLAGS} "${TEST_DIR}/${PROG}" ${LIBS} -o ${CLIENT_BIN}

    for TRIAL in $(seq 1 "$NUM_TRIALS"); do
      THIS_NUM_ITERS=${NUM_ITERS}

      # start a fresh server
      if [[ $FLAGS =~ "withserver" ]]; then
        ${SERVER_EXE} &> /dev/null &
        SERVER_PID=$!
        sleep 2s
      else
        THIS_NUM_ITERS="1"
      fi

      # execute the program N times
      for ITER in $(seq 1 "$THIS_NUM_ITERS"); do

        # make sure the server is still running!
        if [[ $FLAGS =~ "withserver" ]]; then
          kill -0 $SERVER_PID
        fi

        # run the program under 'time'
        ${TIME_EXE} --format="%e" --output="${TIME_OUTPUT_FILE}" ${CLIENT_BIN} &> /dev/null

        ELAPSED_TIME=$(cat "${TIME_OUTPUT_FILE}")
        CSV_ROW=$(printf "%s\n" "$PROG" "$FLAGS" "$TRIAL" "$ITER" "$ELAPSED_TIME"\
                                            | paste -sd ",")

        echo "$CSV_ROW" | tee -a "${CSVFILE}"
      done # iter loop end

      # kill server
      if [[ $FLAGS =~ "withserver" ]]; then
        kill $SERVER_PID
        wait
      fi

    done  # trail loop end

  done
done

# double-check that server is killed
kill ${SERVER_PID}
wait

# clean-up temp file
rm "$TIME_OUTPUT_FILE"
