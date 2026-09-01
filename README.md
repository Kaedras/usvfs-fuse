# USVFS-FUSE

USVFS-FUSE is an experimental reimplementation of [USVFS](https://github.com/ModOrganizer2/usvfs)
using [libfuse](https://github.com/libfuse/libfuse) for use on Linux.

## Table of Contents

1. [Requirements](#requirements)
2. [Building](#building)
    - [Normal build](#normal-build)
      - [Build options](#build-options)
      - [Testing](#testing)
    - [Using cmake workflows](#using-cmake-workflows)
      - [Testing](#testing-1)
3. [Note on unit tests](#note-on-unit-tests)
4. [Benchmarking](#benchmarking)

## Requirements

- CMake >=3.28
- GCC >=16.2, or clang >=22
- libfuse >=3.14
- Qt6 core
- spdlog
- gtest (when building unit tests)
- google benchmark (when building benchmarks)

## Building

### Normal build

```shell
cmake -B build # optionally add e.g. -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
```

#### Build options:

- BUILD_TESTING: build unit tests
- BUILD_BENCHMARKS: build benchmarks
- ENABLE_ASAN: enable address sanitizer
- ENABLE_UBSAN: enable undefined behavior sanitizer

#### Testing

Build with either `-DBUILD_TESTING=ON` or the `linux-testing` preset

```shell
ctest --test-dir build --output-on-failure --repeat until-fail:20 --timeout 5
```

### Using cmake workflows

`CMakePresets.json` contains multiple workflow presets. Run `cmake --workflow --list-presets` to list them. Presets
without the `-no-vcpkg` suffix use vcpkg and require `$VCPKG_ROOT` to be set.

```shell
cmake --workflow --preset build-no-vcpkg
```

```shell
export VCPKG_ROOT=/path/to/vcpkg
cmake --workflow --preset build
```

#### Testing

```shell
cmake --workflow --preset build-and-test-no-vcpkg
```

## Note on unit tests

Unit tests should be run with additional arguments like `--repeat until-fail:20 --timeout 5` as there have been test
failures that only showed up sporadically (1 out of over 10 runs) and/or caused the test not to exit.

## Known issues/limitations

- Some functions haven’t been implemented yet (they may not even be required)
- There may be much room for performance optimization
- Unit tests need to be expanded
- Mounting in a separate user namespace by calling `UsvfsManager::setUseMountNamespace(true)` may require an AppArmor
  rule

## Benchmarking

See `tests/benchmarks/README.md`
