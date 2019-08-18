#!/bin/bash

set -ex

# first arg is the build environment nickname
ENV_KIND=$1

ENV_KIND_OPTIONS="local, docker, rpi, kavon"

if (( $# != 1 )); then
    echo "Must provide a build kind as first arg: $ENV_KIND_OPTIONS"
    exit 1
fi

#################
# check asssumption
if ! echo ./* | grep -q fresh-build.sh; then
  echo "Must run this script in the same directory that it resides in."
  exit 1
fi


############
# always prefer using Ninja if available

if command -v ninja; then
  GENERATOR="Ninja"
  BUILD_CMD="ninja"
else
  NUM_CPUS=$(getconf _NPROCESSORS_ONLN)
  GENERATOR="Unix Makefiles"
  BUILD_CMD="make -j${NUM_CPUS}"
fi

#########
# default build options and setup
BACKENDS="AArch64;AMDGPU;ARM;NVPTX;PowerPC;X86"  # all those that support JIT
TYPE="Release"
PROJECTS="clang;compiler-rt"
OPTIONS="-DCMAKE_INSTALL_PREFIX=../install"
BUILD_TARGETS="install-haloserver install-halomon install-clang install-clang-resource-headers"

# NOTE: we build halo server as an external LLVM project.
OURSELVES="-DLLVM_EXTERNAL_PROJECTS=haloserver -DLLVM_EXTERNAL_HALOSERVER_SOURCE_DIR=$(pwd)"
NETWORK_DIR="-DHALO_NET_DIR=$(pwd)/net"

# environment specific build options / overrides
if [ "${ENV_KIND}" == "docker" ]; then
  OPTIONS="-DLLVM_PARALLEL_LINK_JOBS=2" # want to install system-wide, so we omit the old OPTIONS

elif [ "${ENV_KIND}" == "local" ]; then
  # by default, we install locally anyways.
  :
elif [ "${ENV_KIND}" == "rpi" ]; then
  BACKENDS="Native"
  OPTIONS="${OPTIONS} -DLLVM_USE_LINKER=lld -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_PARALLEL_COMPILE_JOBS=2"

elif [ "${ENV_KIND}" == "kavon" ]; then
  BACKENDS="Native"
  OPTIONS="${OPTIONS} -DLLVM_CCACHE_BUILD=ON -DLLVM_USE_LINKER=gold"

else
  echo "Unknown build kind '${ENV_KIND}'. Options are: $ENV_KIND_OPTIONS"
  exit 1
fi

########
# configure and build!
rm -rf build install
mkdir build install
cd ./build || exit 1
cmake -G "$GENERATOR" ${OURSELVES} ${NETWORK_DIR} -DLLVM_BUILD_LLVM_DYLIB=ON -DLLVM_ENABLE_PROJECTS="$PROJECTS" -DLLVM_TARGETS_TO_BUILD="$BACKENDS" ${OPTIONS} -DCMAKE_BUILD_TYPE=${TYPE} ../llvm-project/llvm

if [ "${ENV_KIND}" == "kavon" ]; then
  exit 0  # only want to configure
fi

# actually perform the build
${BUILD_CMD} ${BUILD_TARGETS}
