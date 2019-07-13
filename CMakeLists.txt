cmake_minimum_required(VERSION 3.8)

project(haloserver
  # VERSION 0.1
  LANGUAGES C CXX
)


############################################
### build options and setup

# make our custom modules available
set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  )

if (NOT HALO_PROTO_DIR)
  message( FATAL_ERROR "Please set HALO_PROTO_DIR to a directory containing protobuf files." )
endif()

file(GLOB HALO_PROTO_FILES "${HALO_PROTO_DIR}/*.proto")


#######################
### find dependencies

find_package(Boost 1.65 COMPONENTS system REQUIRED)
find_package(Protobuf 3 REQUIRED)
llvm_map_components_to_libnames(LLVM_LIBS all) # TODO: make this list shorter?

include_directories(${Protobuf_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
add_definitions(${LLVM_DEFINITIONS} -DGOOGLE_PROTOBUF_NO_RTTI -DBOOST_EXCEPTION_DISABLE -DBOOST_NO_RTTI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

add_subdirectory(tools)
