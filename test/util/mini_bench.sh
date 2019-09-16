#!/bin/bash

# NOTE: run this script while inside the test root directory.

ROOT=$1
OUTFILE="./bench.txt"

if [[ $# -lt 1 ]]; then
  echo "must provide path to root of HALO build / install dir"
  exit 1
fi

SERVER_EXE="$ROOT/bin/haloserver"
CLANG_EXE="$ROOT/bin/clang"
TIME_EXE=`which time`  # do not want built-in bash 'time'

declare -a BENCHMARKS=(
  "bench/c/almabench.c"
  "bench/c/linpack-pc.c"
)

declare -a OPTIONS=(
  "-O0"
  "-O0 -fhalo"
  "-O1"
  "-O1 -fhalo"
)

# overwrite and create the file
echo "minibench session" > ${OUTFILE}

${SERVER_EXE} &
SERVER_PID=$!
sleep 2s

for PROG in ${BENCHMARKS[@]}; do
  echo "${PROG}" >> ${OUTFILE}
  for FLAGS in ${OPTIONS[@]}; do
    echo "${FLAGS}" >> ${OUTFILE}
    ${CLANG_EXE} -DSMALL_PROBLEM_SIZE ${FLAGS} ${PROG} -lm
    ${TIME_EXE} --append --format="%e" --output=${OUTFILE} ./a.out
    ${TIME_EXE} --append --format="%e" --output=${OUTFILE} ./a.out
    ${TIME_EXE} --append --format="%e" --output=${OUTFILE} ./a.out
  done
  echo -e "\n------------------------\n" >> ${OUTFILE}
done

kill ${SERVER_PID}
wait
