

add_executable(simple simple.cpp)

###############
# force linking of the executable with what we specify,
# or else halomon will not actually be included.
set_target_properties(simple PROPERTIES LINK_FLAGS "-Wl,-no-as-needed")

add_dependencies(simple halomon)
target_link_libraries (simple halomon)
