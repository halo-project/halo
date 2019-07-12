


set(SERVER_BIN "halo-server")

add_executable(${SERVER_BIN} HaloServer.cpp)






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
