


set(SERVER_BIN "halo-server")

# perform protobuf & grcp compile step
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_FILES})

add_executable(${SERVER_BIN}
  HaloServer.cpp
  ${GRPC_SRCS}
  ${GRPC_HDRS}
)


target_link_libraries(${SERVER_BIN}
  ${GRPC_LIBRARIES}
)



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
