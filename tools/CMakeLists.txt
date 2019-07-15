set(SERVER_BIN "haloserver")

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${HALO_PROTO_FILES})

add_executable(${SERVER_BIN}
  HaloServer.cpp
  Error.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)

set_property(TARGET ${SERVER_BIN} PROPERTY CXX_STANDARD 14)

target_link_libraries(${SERVER_BIN} ${LLVM_LIBS} ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})

install(TARGETS ${SERVER_BIN}
        COMPONENT ${SERVER_BIN}   # gives a name to this install target for the phony target below.
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

add_custom_target(install-${SERVER_BIN}
  DEPENDS ${SERVER_BIN}
  COMMAND
      "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT="${SERVER_BIN}"
      -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
)


#############################
# delete below here later!
#############################

set(CLIENT_BIN "haloclient")

add_executable(${CLIENT_BIN}
  TestClient.cpp
  Error.cpp
  ${PROTO_SRCS}
  ${PROTO_HDRS}
)

set_property(TARGET ${CLIENT_BIN} PROPERTY CXX_STANDARD 14)

target_link_libraries(${CLIENT_BIN} ${LLVM_LIBS} ${Boost_LIBRARIES} ${Protobuf_LIBRARIES})

install(TARGETS ${CLIENT_BIN}
        COMPONENT ${CLIENT_BIN}   # gives a name to this install target for the phony target below.
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

add_custom_target(install-${CLIENT_BIN}
  DEPENDS ${CLIENT_BIN}
  COMMAND
      "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT="${CLIENT_BIN}"
      -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
)
