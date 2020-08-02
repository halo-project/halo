#!/bin/bash

# quit if anything fails
set -eEuo pipefail

# set -x  # show commands as they're run for debugging

ROOT=$1
NUM_TRIALS=$2   # number of fresh trials, to average the results.
NUM_ITERS=$3    # number of times to run the program _per_ trial

STAMP=$(date +"%m_%d_%Y-%H_%M_%S")
CSVFILE="./results-${STAMP}.csv"

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

BIN_DIR="$ROOT/bin"
SERVER_EXE="$BIN_DIR/haloserver"
TIME_EXE=$(command -v time)  # do not want built-in bash 'time'

if [ -z "$TIME_EXE" ]; then
  echo "GNU time command is missing!"
  exit 1
fi

# the superset of all libs possibly needed by all benchmarks.
LIBS="-lm"

# control problem-size, etc.
PREPROCESSOR_FLAGS="-DSMALL_PROBLEM_SIZE"

# files for managing PGO compilation
PROFILE_RAW_FILE="default.profraw" # the default file that is written to if LLVM_PROFILE_FILE is not set
PROFILE_DATA_FILE="code.profdata"

# get directory of this script
SELF_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
TEST_DIR="$SELF_DIR/.."

declare -a BENCHMARKS=(
  "bench/cpp/matrix.cpp"
  "bench/c/spectralnorm.c"
  "bench/c/oourafft.c"
  "bench/c/n-body.c"
  # "basic/fixed_workload.c"
  # "basic/random_workload.c"
  # "bench/c/almabench.c"
  "bench/c/perlin.c"
  "bench/c/ReedSolomon.c"
)

declare -a AOT_OPTS=(
  # "-O0"
  "-O1"
  "-O2"
  "-O3"
)

declare -a OPTIONS=(
  "none"
  "pgo"
  "-fhalo"
  "withserver -fhalo;--halo-strategy=adapt --halo-threads=2 --halo-metric=calls"
  "withserver -fhalo;--halo-strategy=adapt --halo-threads=2 --halo-metric=ipc"
  "withserver -fhalo;--halo-strategy=jit"
)

# overwrite and create the file

HEADER="program,flags,aot_opt,trial,iter,time"
echo "${HEADER}" | tee "${CSVFILE}"


# we need to send output of the time command to a separate file.
TIME_OUTPUT_FILE=$(mktemp)
SERVER_PID="" # clear it to make sure we don't kill something random!
SERVER_LOG=""

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    if kill -0 $SERVER_PID 2> /dev/null ; then
      kill -9 $SERVER_PID
    fi
  fi
  rm -f "$TIME_OUTPUT_FILE $PROFILE_RAW_FILE $PROFILE_DATA_FILE"
}

# if we hit an error, try to print the server log if it exists, then clean-up
err_handler() {
  echo "minibench error handler invoked!"
  if [[ -n "$SERVER_LOG" && -f "$SERVER_LOG" ]]; then
    echo "Server log preview:"
    tail -n 50 "$SERVER_LOG"
    echo "See more in the log at $SERVER_LOG"
  fi
  cleanup
}

trap cleanup SIGINT EXIT
trap err_handler ERR


for PROG in "${BENCHMARKS[@]}"; do
  for AOT_OPT in "${AOT_OPTS[@]}"; do
    for FLAGS in "${OPTIONS[@]}"; do
      CLIENT_BIN="./a.out"

      if [[ $PROG =~ .*\.cpp$ ]]; then
        CLANG_EXE="${BIN_DIR}/clang++"
      else
        CLANG_EXE="${BIN_DIR}/clang"
      fi

      COMPILE_FLAGS=${FLAGS//withserver/}       # remove 'withserver' from flags
      COMPILE_FLAGS=${COMPILE_FLAGS//none/}     # remove 'none' from flags
      COMPILE_FLAGS=${COMPILE_FLAGS//pgo/}     # remove 'pgo' from flags
      COMPILE_FLAGS=$(echo "$COMPILE_FLAGS" | cut -d ";" -f 1)  # take the first field, (separated by ;)
      COMPILE_FLAGS="$COMPILE_FLAGS $AOT_OPT"   # add aot opt

      ALL_FLAGS="${PREPROCESSOR_FLAGS} ${COMPILE_FLAGS}"

      if [[ $FLAGS == "pgo" ]]; then
        # compile and run once with instrumentation-based profiling
        # based on: https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization

        rm -f ${PROFILE_RAW_FILE} ${PROFILE_DATA_FILE} &> /dev/null

        # compile with profiling enabled
        ${CLANG_EXE} ${ALL_FLAGS} -fprofile-instr-generate "${TEST_DIR}/${PROG}" ${LIBS} -o ${CLIENT_BIN} &> /dev/null

        # run the profiling-enabled binary to generate data
        ${CLIENT_BIN} &> /dev/null

        # turn the "raw" profile data into one suitable for feeding into compiler
        ${BIN_DIR}/llvm-profdata merge -output=${PROFILE_DATA_FILE} ${PROFILE_RAW_FILE} &> /dev/null

        # add profile use flag to compilation flag
        ALL_FLAGS="${ALL_FLAGS} -fprofile-instr-use=${PROFILE_DATA_FILE}"
      fi

      # compile!
      # NOTE: do NOT double-quote compilation flags!!! ignore shellcheck here.
      ${CLANG_EXE} ${ALL_FLAGS} "${TEST_DIR}/${PROG}" ${LIBS} -o ${CLIENT_BIN} &> /dev/null

      for TRIAL in $(seq 1 "$NUM_TRIALS"); do
        THIS_NUM_ITERS=${NUM_ITERS}

        # start a fresh server
        if [[ $FLAGS =~ "withserver" ]]; then
          SERVER_LOG=$(mktemp)
          SERVER_ARGS=$(echo "$FLAGS" | cut -d ";" -f 2)  # 2nd field (separated by ;)
          ${SERVER_EXE} ${SERVER_ARGS} &> "$SERVER_LOG" &
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
          CSV_ROW=$(printf "%s\n" "$PROG" "$FLAGS" "$AOT_OPT" "$TRIAL" "$ITER" "$ELAPSED_TIME"\
                                              | paste -sd ",")

          echo "$CSV_ROW" | tee -a "${CSVFILE}"
        done # iter loop end

        # kill server
        if [[ $FLAGS =~ "withserver" ]]; then
          kill $SERVER_PID
          wait
          rm -f "$SERVER_LOG"
        fi

      done  # trial loop end

    done # flag loop end
  done # aot opt loop end
done #prog loop end

cleanup