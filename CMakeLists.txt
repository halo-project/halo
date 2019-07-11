cmake_minimum_required(VERSION 3.8)
project(Halo)


############################################
### build options

option(HALO_RPI "Flag indicating a build on Raspberry Pi." OFF)
option(HALO_KAVON "Flag indicating a build on Kavon's laptop." OFF)


############################################
### "superbuild" configuration for LLVM

# defaults
set(LLVM_ENABLE_PROJECTS "clang;compiler-rt")
set(LLVM_TARGETS_TO_BUILD "AArch64;AMDGPU;ARM;NVPTX;PowerPC;X86") # all supporting JIT

# overrides
if(HALO_RPI)
  set(LLVM_TARGETS_TO_BUILD "Native")
elseif(HALO_KAVON)
  set(LLVM_TARGETS_TO_BUILD "Native")
  set(LLVM_CCACHE_BUILD ON)
  set(LLVM_USE_LINKER gold)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/llvm)


###########################################
### find other dependencies of halo server

find_package(Protobuf 3 REQUIRED)

set(gRCP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(gRCP REQUIRED)


###################
### Setup compiler flags

# set the build type, if not already set.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# needed for colors when using Ninja
# source: https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

add_definitions(-DBOOST_NO_RTTI -DBOOST_NO_EXCEPTIONS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -Wall -Wextra -Wpedantic -Werror=return-type -Wno-unused-parameter")
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_subdirectory(tools)
# add_subdirectory(test)
