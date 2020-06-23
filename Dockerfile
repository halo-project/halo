FROM ubuntu:20.04

### NOTES
# 1. In order to fully test the build, you must run the image
#    with:  --cap-add sys_admin
#    because clients of the halo server use perf_events.
#    If you are just running Halo Server, this is not required and it is
#    reccomended to _not_ enable sys_admin.
#
# 2. For self: https://docs.docker.com/develop/develop-images/dockerfile_best-practices/
#    for tips on writing a good docker file.

# tell tzdata we can't answer its questions about time zones!
ARG DEBIAN_FRONTEND=noninteractive

# install dependencies.
RUN apt-get update && apt-get install --no-install-recommends -y \
      g++ \
      libboost-system-dev \
      libboost-graph-dev \
      libgsl-dev \
      libpfm4-dev \
      ninja-build \
      cmake \
      ccache \
      \
      # for our gitlab ci script and xgboost download & build
      git \
      ca-certificates \
      make \
      \
      # for minibench (data collection only)
      time \
      \
      # for lit
      python2 \
      python-is-python2 \
      \
      zlib1g \
      libprotobuf-dev \
      protobuf-compiler \
      libtinfo-dev \
  && rm -rf /var/lib/apt/lists/*

# copy over source code to the image
COPY . /tmp/halo

# set the working directory
WORKDIR /tmp/halo

# obtain XGBoost sources
RUN ./xgboost/get.sh

# build
RUN ./fresh-build.sh docker

# when the image is run, start halo server by default.
CMD haloserver
