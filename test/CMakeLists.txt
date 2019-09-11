
include(Python)

find_python_module(lit REQUIRED)

set(TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# NOTE: if you want to shuffle the tests, add --shuffle
add_custom_target(test-halo
                  COMMAND ${PYTHON_EXEC} -m lit.main ${TEST_ROOT} -v --time-tests -j 1
                  COMMENT "running Halo regression test suite..."
                  USES_TERMINAL
                  )

add_dependencies(test-halo haloserver halomon clang FileCheck not)

configure_file("${TEST_ROOT}/lit.cfg.in" "${TEST_ROOT}/lit.cfg")
