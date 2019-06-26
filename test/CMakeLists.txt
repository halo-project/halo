
# TODO: link in halomon with --no-as-needed or compile
# the test with an artifical dependency on the library.
add_executable(simple simple.cpp)
add_dependencies(simple halomon)
target_link_libraries (simple halomon)
