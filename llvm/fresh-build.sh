#!/bin/bash


#################
# check asssumption
ls | grep -q fresh-build.sh
if [ $? -ne 0 ]; then
  echo "Must run this script in the same directory that it resides in."
  exit 1
fi


############
# prefer using Ninja if available
command -v ninja
if [ $? -eq 0 ]; then
  GENERATOR="Ninja"
  BUILD_CMD="ninja install"
else
  NUM_CPUS=`getconf _NPROCESSORS_ONLN`
  GENERATOR="Unix Makefiles"
  BUILD_CMD="make install -j${NUM_CPUS}"
fi

LLVM_PROJECTS="clang"

########
# configure and build
rm -rf build install
mkdir build install
cd ./build
cmake -G "$GENERATOR" -DLLVM_ENABLE_PROJECTS="$LLVM_PROJECTS" -DLLVM_USE_LINKER=gold -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install ../src/llvm
$BUILD_CMD
