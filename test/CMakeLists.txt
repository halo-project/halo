set(TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# NOTE: if you want to shuffle the tests, add --shuffle
add_custom_target(test-halo
                  COMMAND ${CMAKE_BINARY_DIR}/bin/llvm-lit ${TEST_ROOT} -vv --time-tests -j 1
                  COMMENT "running Halo regression test suite..."
                  USES_TERMINAL
                  )


# NOTE: we also depend on llvm-lit, but it's automatically built during configure
# so long as LLVM_INCLUDE_UTILS option is set, which it is by default.
add_dependencies(test-halo haloserver halomon clang FileCheck not)

configure_file("${TEST_ROOT}/lit.cfg.in" "${TEST_ROOT}/lit.cfg")
