#!/bin/bash

# quit if anything fails
set -eEuo pipefail

# suppresses all output from the command given to it,
# and just prints the running time in seconds

TIME_OUT="$(mktemp)"
TIME_EXE=$(command -v time)

if [ -z "$TIME_EXE" ]; then
  echo "GNU time command is missing!"
  exit 1
fi

$TIME_EXE --format="%e" --output="$TIME_OUT" "$@" &> /dev/null

cat "$TIME_OUT"
rm "$TIME_OUT"
