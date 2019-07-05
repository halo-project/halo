#!/bin/bash


#################
# check asssumption
ls | grep -q fresh-build-rpi.sh
if [ $? -ne 0 ]; then
  echo "Must run this script in the same directory that it resides in."
  exit 1
fi

############
# prefer using Ninja if available
command -v ninja
if [ $? -eq 0 ]; then
  GENERATOR="Ninja"
  # BUILD_CMD="ninja install"
else
  NUM_CPUS=`getconf _NPROCESSORS_ONLN`
  GENERATOR="Unix Makefiles"
  # BUILD_CMD="make install -j${NUM_CPUS}"
fi

####
# select what parts of llvm we want
PROJECTS="clang;compiler-rt"
BACKENDS="Native"

OPTIONS="-DLLVM_USE_LINKER=lld -DCMAKE_BUILD_TYPE=Release -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_PARALLEL_COMPILE_JOBS=2"

########
# configure
rm -rf build install
mkdir build install
cd ./build
cmake -G "$GENERATOR" ${OPTIONS} -DLLVM_ENABLE_PROJECTS="$PROJECTS" -DLLVM_TARGETS_TO_BUILD="$BACKENDS" -DCMAKE_INSTALL_PREFIX=../install ../src/llvm
# $BUILD_CMD
