Halo: Wholly Adaptive LLVM Optimizer
==========================================

[![pipeline status](https://gitlab.com/kavon1/halo/badges/master/pipeline.svg)](https://gitlab.com/kavon1/halo/commits/master)

## Project Abstract

Low-level languages like C, C++, and Rust are the language-of-choice for
performance-sensitive applications and have major implementations that are based
on the LLVM compiler framework. The heuristics and trade-off decisions used to
guide the static optimization of such programs during compilation can be
automatically tuned during execution (online) in response to their actual
execution environment. The main objective of this project is to explore what is
possible in the space of **online adaptive optimization for LLVM-based language
implementations**.

This project differs from the usual application of runtime systems employing JIT
compilation in that we are trying to optimize programs even if they already have
very little interpretive overhead, e.g., no dynamic types. Thus, our focus is on
trying to profitably **tune the compiler optimizations applied to the code 
while the program is running**.
This is in contrast to traditional offline tuning where a sample workload and
hours of time are required to perform the tuning prior to the deployment of the
software, and afterwards the tuning remains fixed.


## What's Here?

There are three major components to the Halo project:

1. `halomon`, aka the Halo Monitor, which is a library that is linked into your
executable to create a Halo-enabled binary. This component lives under
`llvm-project/compiler-rt/lib/halomon`. This component mainly performs profiling 
(currently **Linux only**) and live code patching.

2. `haloserver`, aka the Halo Optimization Server, to which the halo-enabled
binaries connect in order to recieve newly-generated code that is (hopefully)
better optimized. This component lives under `tools/haloserver` and `include`.

3. A special version of `clang` that supports the `-fhalo` flag in order to
produce Halo-enabled binaries. This is in the usual place `llvm-project/clang`.


## Building

We offer Docker images with Halo pre-installed, so "building" amounts
generally should amount to downloading the image. This is the reccomended way to obtain
Halo:

```bash
$ docker pull registry.gitlab.com/kavon1/halo:latest
```

Please note that by using the pre-built Docker image, you'll be required to have
Linux kernel version 4.15 or newer, because our continuous integration machine has
that version (see [Issue #5](https://github.com/halo-project/halo/issues/5)).

If you end up building from source (or building the Docker image locally), 
then only Linux kernel version 3.4 or newer is required.


### Docker

By default, when you run the Docker image it will launch an instance
of `haloserver` with the arguments you pass it:

```bash
$ docker run registry.gitlab.com/kavon1/halo:latest # <arguments to haloserver>
```

Pass `--help` to `haloserver` to get information about some of its options.

If you want to compile and run a Halo-enabled binary within the Docker
container, you'll need to provide extra permissions, `--cap-add sys_admin` when
you execute `docker`. These permissions are required for that binary to use
Linux `perf_events`. Please note that if you *just* want to run `haloserver` in
the container, then the extra permissions are *not* required.

If you want to build the Docker image locally, that should just amount to running
`docker build` in the root of the repository.


### Building From Source

Check the `Dockerfile` for dependencies you may need. Once your system has those
satisfied, at the root of the cloned repository you can run:

```bash
$ ./fresh-build.sh kavon
$ cd build
$ make clang halomon haloserver
```

Replace `make` with `ninja` if you have that that installed, as the script will prefer Ninja.
Please note that you'll end up with a debug build with logging enabled under `build/bin`.
I do not currently have a build setup in that script for real usage.


## Usage

Please keep in mind that this project is still in an early-development phase.
Currently, Halo acts as a simple tiered JIT compilation system with sampling-based
profiling and compilation that happens on a server.https://github.com/halo-project/halo

To produce a Halo-enabled executable, simply add `-fhalo` to your invocation of Halo's `clang`:

```bash
$ clang -fhalo program.c -o program
```

Upon launching the `program`, a thread for the Halo Monitor will be spawned prior to `main` running.
If the monitor does not find the Halo Server at `127.0.0.1:29000` (currently, [over an unencrypted TCP connection](https://github.com/halo-project/halo/issues/2)) then the monitor goes inactive.
Thus, you will want to have the Halo Server running ahead of time.

Generally you can run `haloserver` with no arguments, but see `--help` for more options.
