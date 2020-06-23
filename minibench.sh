#!/bin/bash

# quit if anything fails
set -euo pipefail

./test/util/mini_bench.sh ./build 5 10