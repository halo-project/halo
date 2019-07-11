# Look for gRCP installation.
# unfortunately we can't use find_package() for older
# versions of gRCP, such as 1.3.2 as found on Ubuntu 18.04
# because they did not provide a gRCPConfig.cmake file in
# their installation.
#
# Defines the following:
#
#  gRCP_FOUND
#  gRCP_INCLUDE_DIR
#  gRCP_LIBRARIES

# NOTE: newer versions of gRCP use 'grcpcpp' instead of 'grcpc++' for headers.
find_path(grcp_INCLUDE
  NAMES grpc++/grpc++.h
)

find_library(grcpxx_LIB
  NAMES grpc++
)

set(gRCP_INCLUDE_DIR ${grcp_INCLUDE})
set(gRCP_LIBRARIES    ${grcpxx_LIB})

message(STATUS "gRCP include dir: " ${gRCP_INCLUDE_DIR})
message(STATUS "gRCP libraries: " ${gRCP_LIBRARIES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gRCP
  FOUND_VAR gRCP_FOUND
  REQUIRED_VARS gRCP_INCLUDE_DIR gRCP_LIBRARIES
)
