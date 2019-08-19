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

add_subdirectory(tools/haloserver)
add_subdirectory(test)
