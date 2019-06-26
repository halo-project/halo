

# find dependencies
# TODO: link in boost_system. might just be to add "-lboost_system" here.
find_library(PFM_LIB pfm)

# create the halomon library
add_library(halomon SHARED monitor.cpp)

# link in dependencies
target_link_libraries(halomon ${PFM_LIB})

install(TARGETS halomon DESTINATION lib/)
