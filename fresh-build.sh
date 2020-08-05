#!/bin/bash

set -ex

# first arg is the build environment nickname
ENV_KIND=$1

ENV_KIND_OPTIONS="Options are: local, docker, rpi, kavon. You can also append -debug or -bench to those for different purposes."

if (( $# != 1 )); then
  set +x
  echo "Must provide a build kind as first arg: $ENV_KIND_OPTIONS"
  exit 1
fi

#################
# check asssumption
if ! echo ./* | grep -q fresh-build.sh; then
  set +x
  echo "Must run this script in the same directory that it resides in."
  exit 1
fi


############
# always prefer using Ninja if available

NUM_CPUS=$(getconf _NPROCESSORS_ONLN)
if command -v ninja; then
  GENERATOR="Ninja"
  BUILD_CMD="ninja"
else
  GENERATOR="Unix Makefiles"
  BUILD_CMD="make -j${NUM_CPUS}"
fi

##################
# make sure xgboost is built. we assume the sources have been pulled to 'root' already
pushd xgboost/root || exit 1
if [[ ${ENV_KIND} =~ rpi.* ]]; then
  XG_BUILD_CPUS="2"
else
  XG_BUILD_CPUS=$((NUM_CPUS / 2 + 1))
fi
make -j ${XG_BUILD_CPUS} || (echo "did you run xgboost/get.sh to download the sources?" && exit 1)
popd || exit 1



#########
# default build options and setup
BACKENDS="AArch64;AMDGPU;ARM;NVPTX;PowerPC;X86"  # all those that support JIT
PROJECTS="clang;compiler-rt"
OPTIONS="-DCMAKE_INSTALL_PREFIX=../install"
BUILD_TARGETS="install-haloserver install-halomon install-llvm-profdata install-compiler-rt install-clang install-clang-resource-headers"

# NOTE: we build halo server as an external LLVM project.
OURSELVES="-DLLVM_EXTERNAL_PROJECTS=haloserver -DLLVM_EXTERNAL_HALOSERVER_SOURCE_DIR=$(pwd)"
NETWORK_DIR="-DHALO_NET_DIR=$(pwd)/net"

# without any dash-option appended, we compile Release + Asserts
TYPE="Release"
DEVELOPMENT_FLAGS="-DLLVM_ENABLE_ASSERTIONS=ON"

# handle variants of build
if [[ ${ENV_KIND} =~ .*-debug ]]; then
  TYPE="Debug"  # NOTE: this will make linking a lot harder, so avoid it on low RAM machines!
  DEVELOPMENT_FLAGS="$DEVELOPMENT_FLAGS -DHALOSERVER_VERBOSE=ON -DHALOMON_VERBOSE=ON"
elif [[ ${ENV_KIND} =~ .*-bench ]]; then
  DEVELOPMENT_FLAGS="-DLLVM_ENABLE_ASSERTIONS=OFF -DHALOSERVER_VERBOSE=OFF"
fi

# environment specific build options / overrides
if [[ ${ENV_KIND} =~ docker.* ]]; then
  # want to install system-wide, so we overwrite the existing OPTIONS
  OPTIONS="-DLLVM_USE_LINKER=gold -DLLVM_PARALLEL_LINK_JOBS=4 -DLLVM_CCACHE_BUILD=ON $DEVELOPMENT_FLAGS"

elif [[ ${ENV_KIND} =~ local.* ]]; then
  # install locally
  OPTIONS="${OPTIONS} $DEVELOPMENT_FLAGS"

elif [[ ${ENV_KIND} =~ rpi.* ]]; then
  BACKENDS="Native"
  OPTIONS="${OPTIONS} -DLLVM_USE_LINKER=lld -DLLVM_CCACHE_BUILD=ON -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_PARALLEL_COMPILE_JOBS=2 $DEVELOPMENT_FLAGS"
  BUILD_TARGETS="install-halomon install-llvm-profdata install-compiler-rt install-clang install-clang-resource-headers"  # no server needed

elif [[ ${ENV_KIND} =~ kavon.* ]]; then
  BACKENDS="X86;ARM"
  OPTIONS="${OPTIONS} -DLLVM_USE_LINKER=gold -DLLVM_CCACHE_BUILD=ON $DEVELOPMENT_FLAGS"

else
  set +x
  echo "Unknown build kind '${ENV_KIND}'. Options are: $ENV_KIND_OPTIONS"
  exit 1
fi

########
# configure and build!
rm -rf build install
mkdir build install
cd ./build || exit 1
cmake -G "$GENERATOR" ${OURSELVES} ${NETWORK_DIR} -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DLLVM_ENABLE_PROJECTS="$PROJECTS" -DLLVM_TARGETS_TO_BUILD="$BACKENDS" ${OPTIONS} \
    -DCMAKE_BUILD_TYPE=${TYPE} ../llvm-project/llvm

if [[ ${ENV_KIND} =~ kavon.* ]]; then
  set +x
  echo "CMake configuration complete, Kavon. Invoke build targets yourself in build dir."
  exit 0  # only want to configure
fi

# actually perform the build
${BUILD_CMD} ${BUILD_TARGETS}
