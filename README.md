# USVFS-FUSE

USVFS-FUSE is an experimental reimplementation of [USVFS](https://github.com/ModOrganizer2/usvfs)
using [libfuse](https://github.com/libfuse/libfuse) for use on Linux.

## Requirements

- CMake 3.31
- GCC 14.2
- libfuse 3.14
- icu
- spdlog
- gtest (when building unit tests)
- google benchmark (when building performance tests)

## Building

```shell
mkdir build && cd build
cmake .. # optionally add e.g. -DBUILD_TESTING=ON
make -j$(nproc)
```

optional build options:
- BUILD_TESTING=ON/OFF: build unit tests
- BUILD_PERF_TESTS=ON/OFF: build performance tests

## Known issues/limitations

- Some functions haven’t been implemented yet (they may not even be required)
- There may be much room for performance optimisation
- Unit tests need to be expanded
