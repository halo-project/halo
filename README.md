Halo: Wholly Adaptive LLVM Optimizer
==========================================

[![pipeline status](https://gitlab.com/kavon1/halo/badges/master/pipeline.svg)](https://gitlab.com/kavon1/halo/commits/master)

## Project Abstract

Low-level languages like C, C++, and Rust are the language-of-choice for
performance-sensitive applications and have major implementations that are based
on the LLVM compiler framework. The heuristics and trade-off decisions used to
guide the static optimization of these programs during compilation can be
automatically tuned during execution (online) in response to their actual
execution environment. The main objective of this project is to explore what is
possible in the space of online adaptive optimization for LLVM-based language
implementations.

This project differs from the usual application of runtime systems employing JIT
compilation in that we are trying to optimize programs that already have very
little interpretive overhead, e.g., no dynamic types. Instead, we are
**automatically tuning the compiler optimizations**, which is something one can
do offline but will specialize the tuning to a certian granularity and is
generally harder to setup and run.

## What's Here?

There are three major components to the Halo project:

1. `halomon`, aka the Halo Monitor, which is a library that is linked into your
executable to create a Halo-enabled binary. This component lives under
`llvm-project/compiler-rt/lib/halomon`.

2. `haloserver`, aka the Halo Optimization Server, to which the halo-enabled
binaries connect in order to recieve newly-generated code that is (hopefully)
better optimized. This component lives under `tools/haloserver` and `include`.

3. A special version of `clang` that supports the `-fhalo` flag in order to
produce Halo-enabled binaries. This is in the usual place `llvm-project/clang`.


## Building

We offer Docker images with Halo pre-installed so "building" amounts
to just downloading the image. This is the reccomended way to get going.

### Docker

By default, when you run the Docker image it will launch an instance
of `haloserver` with the arguments you pass it:

```
docker run registry.gitlab.com/kavon1/halo:latest # <arguments to haloserver>
```

Pass `--help` to `haloserver` to get information about some of its options.

If you want to compile and run a Halo-enabled binary within the Docker
container, you'll need to provide extra permissions, `--cap-add sys_admin` when
you execute `docker`. These permissions are required for that binary to use
Linux `perf_events`. Please note that if you *just* want to run `haloserver` in
the container, then the extra permissions are *not* required.

### Building From Source

Check the `Dockerfile` for dependencies you may need. Once your system has those
satisfied, at the root of the cloned repository you can run:

```
./fresh-build.sh kavon
```

Please note that you'll end up with a debug build with logging enabled. I do not
currently have a build setup in that script for a "release" or benchmarking yet.

## Usage

TODO: basic haloserver / compile example.