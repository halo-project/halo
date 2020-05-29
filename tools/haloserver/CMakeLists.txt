set(SERVER_BIN "haloserver")



if (NOT HALO_NET_DIR)
  message( FATAL_ERROR "Please set HALO_NET_DIR to a directory containing networking files." )
endif()

set(HALO_PROTO_FILES
  ${HALO_NET_DIR}/Messages.proto
)

include_directories(${HALO_NET_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../../include" "${CMAKE_CURRENT_SOURCE_DIR}/../../rllib/src")


option(HALOSERVER_VERBOSE "If set to ON, then debug / logging will be output." ON)
if (HALOSERVER_VERBOSE)
  add_definitions(-DHALO_VERBOSE)
endif()

#######################
### find dependencies

find_package(Boost 1.65 COMPONENTS system graph REQUIRED)
find_package(Protobuf 3 REQUIRED)
find_package(GSL 2.4 REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${GSL_INCLUDE_DIR})
add_definitions(${LLVM_DEFINITIONS} -DGOOGLE_PROTOBUF_NO_RTTI -DBOOST_EXCEPTION_DISABLE -DBOOST_NO_RTTI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fmax-errors=1")
# NOTE:
#   - exceptions are ENABLED in the server because rllib uses them. However we still ask
#     for boost to not use exceptions because our use of boost code is partially shared
#     with the monitor (for ASIO), which should not exceptions since it's embedded in LLVM.
#
#   - because c++ compiler error messages are insane,  -fmax-errors=1

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${HALO_PROTO_FILES})

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_executable(${SERVER_BIN}
  Bandit.cpp
  CallGraph.cpp
  CallingContextTree.cpp
  ClientGroup.cpp
  ClientSession.cpp
  CodeRegionInfo.cpp
  CompilationPipeline.cpp
  HaloServer.cpp
  Knob.cpp
  KnobSet.cpp
  MDUtils.cpp
  PerformanceData.cpp
  Profiler.cpp
  ProgramInfoPass.cpp
  TuningSection.cpp
  ${HALO_NET_DIR}/Logging.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)

# NOTE: rlllib requires c++17
set_property(TARGET ${SERVER_BIN} PROPERTY CXX_STANDARD 17)

target_link_libraries(${SERVER_BIN} PRIVATE ${Boost_LIBRARIES} ${Protobuf_LIBRARIES} ${GSL_LIBRARIES})
llvm_config(${SERVER_BIN} USE_SHARED) # since halomon requires libLLVM, we use it here too.

install(TARGETS ${SERVER_BIN}
        COMPONENT ${SERVER_BIN}   # gives a name to this install target for the phony target below.
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

# make the install target aware of the JSON file
install(FILES server-config.json
        COMPONENT ${SERVER_BIN}
        DESTINATION bin)

# also copy the JSON file into build bin dir. we need to tell cmake
# that the executable depends on the JSON file in the source tree,
# so that changes to it trigger a re-copy.
add_custom_target(haloserver-configfile DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server-config.json)
add_dependencies(${SERVER_BIN} haloserver-configfile)
add_custom_command(
        TARGET haloserver-configfile
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/server-config.json
                ${CMAKE_BINARY_DIR}/bin/server-config.json)

# the install-haloserver target
add_custom_target(install-${SERVER_BIN}
  DEPENDS ${SERVER_BIN} install-LLVM
  COMMAND
      "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT="${SERVER_BIN}"
      -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
)
