set(SERVER_BIN "haloserver")

if (NOT HALO_NET_DIR)
  message( FATAL_ERROR "Please set HALO_NET_DIR to a directory containing networking files." )
endif()

file(GLOB HALO_PROTO_FILES "${HALO_NET_DIR}/*.proto")
include_directories(${HALO_NET_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../../include")


#######################
### find dependencies

find_package(Boost 1.65 COMPONENTS system REQUIRED)
find_package(Protobuf 3 REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
add_definitions(${LLVM_DEFINITIONS} -DGOOGLE_PROTOBUF_NO_RTTI -DBOOST_EXCEPTION_DISABLE -DBOOST_NO_RTTI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${HALO_PROTO_FILES})

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_executable(${SERVER_BIN}
  HaloServer.cpp
  Error.cpp
  CodeRegionInfo.cpp
  ClientGroup.cpp
  ClientSession.cpp
  CompilationPipeline.cpp
  MDUtils.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)

set_property(TARGET ${SERVER_BIN} PROPERTY CXX_STANDARD 14)

target_link_libraries(${SERVER_BIN} PRIVATE ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})
llvm_config(${SERVER_BIN} USE_SHARED) # since halomon requires libLLVM, we use it here too.

install(TARGETS ${SERVER_BIN}
        COMPONENT ${SERVER_BIN}   # gives a name to this install target for the phony target below.
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

add_custom_target(install-${SERVER_BIN}
  DEPENDS ${SERVER_BIN} install-LLVM
  COMMAND
      "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT="${SERVER_BIN}"
      -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
)
