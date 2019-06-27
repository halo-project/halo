


# given a name and a file to compile.
function (make_halomon_exe name file)

  add_executable(${name} ${file})
  add_executable(${name}-bare ${file})

  ###############
  # force linking of the executable with what we specify,
  # or else halomon will not actually be included.

  # HACK: we should not be using --no-as-needed (i.e., LINK_WHAT_YOU_USE)
  # for all libraries, only halomon!
  # Only CMake >= 3.13 seems to have what we would need, which is
  # `target_link_options`, in order to have:
  #          -Wl,-no-as-needed -lhalomon -Wl,-as-needed
  # specified AFTER the rpath stuff so it works.
  #
  # For now, to make comparisons fair between the halomon vs non-halomon
  # tests, we turn on --no-as-needed for the bare version too.
  # See here for previous investigation:
  #     https://github.com/halo-project/halo/issues/1
  #
  set_target_properties(${name} PROPERTIES LINK_WHAT_YOU_USE True)
  set_target_properties(${name}-bare PROPERTIES LINK_WHAT_YOU_USE True)

  add_dependencies(${name} halomon)
  target_link_libraries (${name} halomon)

endfunction()


make_halomon_exe(noop noop.cpp)
make_halomon_exe(linear_hot linear_hot.cpp)
