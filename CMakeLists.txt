cmake_minimum_required(VERSION 3.8)
project(HaloServer)


############################################
### build options

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


############################################
### "superbuild" configuration for LLVM

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/llvm)


###########################################
### find other dependencies of halo server

find_package(GRCP REQUIRED)
include_directories(${GRPC_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DGOOGLE_PROTOBUF_NO_RTTI)

# include_directories(${Protobuf_INCLUDE_DIRS} ${gRCP_INCLUDE_DIR})


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
