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

####
# select what parts of llvm we want
PROJECTS="clang;compiler-rt"
BACKENDS="AArch64;AMDGPU;ARM;NVPTX;PowerPC;X86"

# build speed options / optimization.
OPTIONS="-DLLVM_CCACHE_BUILD=ON -DLLVM_USE_LINKER=lld"

########
# configure and build
rm -rf build install
mkdir build install
cd ./build
cmake -G "$GENERATOR" -DLLVM_ENABLE_PROJECTS="$PROJECTS" -DLLVM_TARGETS_TO_BUILD="$BACKENDS" ${OPTIONS} -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install ../src/llvm
$BUILD_CMD
