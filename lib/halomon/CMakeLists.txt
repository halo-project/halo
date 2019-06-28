
#####################
# find dependencies

find_library(PFM_LIB pfm)  # http://perfmon2.sourceforge.net/

find_package(Boost 1.65 COMPONENTS system REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# include our local 'include' dir
include_directories(SYSTEM ../../include)

# setup to use LLVM
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(LLVM_LIBS all)


#####################
# create the halomon shared library
add_library(halomon SHARED
            Monitor.cpp
            Error.cpp
            Profiler.cpp
            LinuxPerfEvents.cpp
           )

#####################
# link in dependencies
target_link_libraries(halomon PRIVATE ${LLVM_LIBS} ${PFM_LIB} ${Boost_LIBRARIES})

#####################
# install it
install(TARGETS halomon DESTINATION lib)
