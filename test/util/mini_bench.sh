#!/bin/bash

# NOTE: run this script while inside the test root directory.
set -o xtrace

ROOT=$1
OUTFILE="./bench.txt"

if [[ $# -lt 1 ]]; then
  echo "must provide path to root of HALO build / install dir"
  exit 1
fi

SERVER_EXE="$ROOT/bin/haloserver"
CLANG_EXE="$ROOT/bin/clang++"
TIME_EXE=$(which time)  # do not want built-in bash 'time'

declare -a BENCHMARKS=(
  "bench/cpp/oopack_v1p8.cpp"
)

declare -a OPTIONS=(
  "-O0"
  "-fhalo -O0"
  "-O1"
  "-fhalo -O1"
  "-O2"
  "-fhalo -O2"
  "-O3"
  "-fhalo -O3"
)

# overwrite and create the file
echo "** minibench test **" > ${OUTFILE}

for PROG in "${BENCHMARKS[@]}"; do
  echo "${PROG}" >> ${OUTFILE}
  for FLAGS in "${OPTIONS[@]}"; do
    echo -e "\n${FLAGS}" >> ${OUTFILE}
    ${CLANG_EXE} -DSMALL_PROBLEM_SIZE ${FLAGS} "${PROG}"

    for TRIAL in {1..2}; do
      # ${SERVER_EXE} --no-persist &
      # SERVER_PID=$!
      # sleep 2s
      ${TIME_EXE} --append --format="%e" --output=${OUTFILE} ./a.out
      # kill $SERVER_PID
      # wait
    done

  done
  echo -e "\n------------------------\n" >> ${OUTFILE}
done

kill ${SERVER_PID}
wait
