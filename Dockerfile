FROM ubuntu:18.04

### NOTES
# 1. You must run this image with:  --cap-add sys_admin
#    in order to use perf_events.
#
# 2. see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/
#    for tips on writing a good docker file.

# install dependencies.
RUN apt-get update && apt-get install --no-install-recommends -y \
      g++ \
      libboost-system-dev \
      libpfm4-dev \
      ninja-build \
      cmake \
      python2.7 \
      zlib1g \
      libprotobuf-dev \
      protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

# copy over source code to the image
COPY . /tmp/halo

# set the working directory
WORKDIR /tmp/halo

# build and clean-up
# TODO: run test suite before cleanup!
RUN ./fresh-build.sh docker \
    && rm -rf llvm-project build

# when the image is run, start halo server by default.
# CMD halo-server
