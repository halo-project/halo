cmake_minimum_required(VERSION 3.8)
project(HaloServer)


############################################
### build options

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# This is set globally here so it is accessable by halomon and halo server.
# I don't know a better way to share these specs with both components
# without copying it.
file(GLOB HALO_PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")


############################################
### "superbuild" configuration for LLVM

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/llvm)


###########################################
### find other dependencies of halo server

find_package(Protobuf 3 REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})


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

add_definitions(-DBOOST_NO_RTTI -DBOOST_NO_EXCEPTIONS -DGOOGLE_PROTOBUF_NO_RTTI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -Wall -Wextra -Wpedantic -Werror=return-type -Wno-unused-parameter")
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# add_subdirectory(lib)
add_subdirectory(tools)
