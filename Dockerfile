FROM ubuntu:18.04

### NOTES
# 1. In order to fully test the build, you must run the image
#    with:  --cap-add sys_admin
#    because clients of the halo server use perf_events.
#    If you are just running Halo Server, this is not required and it is
#    reccomended to _not_ enable sys_admin.
#
# 2. For self: https://docs.docker.com/develop/develop-images/dockerfile_best-practices/
#    for tips on writing a good docker file.

# install dependencies.
RUN apt-get update && apt-get install --no-install-recommends -y \
      g++ \
      libboost-system-dev \
      libboost-graph-dev \
      libpfm4-dev \
      ninja-build \
      cmake \
      ccache \
      git \
      python2.7 \
      python-pip \
      zlib1g \
      libprotobuf-dev \
      protobuf-compiler \
      libtinfo-dev \
  # TODO: I'm not sure if pip is needed anymore since we use in-tree lit now.
  && python -m pip install --no-cache --upgrade pip setuptools wheel \
  && rm -rf /var/lib/apt/lists/*

# copy over source code to the image
COPY . /tmp/halo

# set the working directory
WORKDIR /tmp/halo

# build
RUN ./fresh-build.sh docker

# when the image is run, start halo server by default.
CMD haloserver
