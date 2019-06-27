


# given a name and a file to compile.
function (make_halomon_exe name file)

  add_executable(${name} ${file})
  add_executable(${name}-bare ${file})

  ###############
  # force linking of the executable with what we specify,
  # or else halomon will not actually be included.
  set_target_properties(${name} PROPERTIES LINK_FLAGS "-Wl,-no-as-needed")

  add_dependencies(${name} halomon)
  target_link_libraries (${name} halomon)

endfunction()


make_halomon_exe(noop noop.cpp)
make_halomon_exe(linear_hot linear_hot.cpp)
