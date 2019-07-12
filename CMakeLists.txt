cmake_minimum_required(VERSION 3.8)

project(haloserver
  # VERSION 0.1
  LANGUAGES C CXX
)


############################################
### build options and setup

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (NOT HALO_PROTO_DIR)
  message( FATAL_ERROR "Please set HALO_PROTO_DIR to a directory containing protobuf files." )
endif()

file(GLOB HALO_PROTO_FILES "${HALO_PROTO_DIR}/*.proto")


###########################################
### find other dependencies of halo server

find_package(Protobuf 3 REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

add_subdirectory(tools)
