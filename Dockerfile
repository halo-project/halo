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
      make \
    && rm -rf /var/lib/apt/lists/*

# copy over source code to the image
COPY . /tmp/perf_test

# set the working directory
WORKDIR /tmp/perf_test

# build
# TODO: update for new cmake build system
RUN make

# when the image is run, execute the test program.
CMD ./test
