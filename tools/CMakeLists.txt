set(SERVER_BIN "haloserver")


# include LLVM itself
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(LLVM_LIBS all) # TODO: make this list shorter?

# DEBUG
message(WARNING ${LLVM_MAIN_INCLUDE_DIR})
message(WARNING ${LLVM_DEFINITIONS})

add_executable(${SERVER_BIN}
  HaloServer.cpp
)

# target_include_directories(${SERVER_BIN} PRIVATE ${LLVM_MAIN_INCLUDE_DIR})
target_link_libraries(${SERVER_BIN} ${LLVM_LIBS})

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
