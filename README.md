# USVFS-FUSE

USVFS-FUSE is an experimental reimplementation of [USVFS](https://github.com/ModOrganizer2/usvfs)
using [libfuse](https://github.com/libfuse/libfuse) for use on Linux.

## Requirements

- CMake >=3.28
- GCC >=16.2, or clang >=22
- libfuse >=3.14
- Qt6 core
- spdlog
- gtest (when building unit tests)
- google benchmark (when building benchmarks)

## Building

```shell
cmake -B build # optionally add e.g. -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
```

### Build options:

- BUILD_TESTING: build unit tests
- BUILD_BENCHMARKS: build benchmarks
- ENABLE_ASAN: enable address sanitizer

## Known issues/limitations

- Some functions haven’t been implemented yet (they may not even be required)
- There may be much room for performance optimization
- Unit tests need to be expanded
- Mounting in a separate user namespace by calling `UsvfsManager::setUseMountNamespace(true)` may require an AppArmor
  rule

## Note on unit tests

Unit tests should be run with additional arguments like `--repeat until-fail:20 --timeout 5` as there have been test
failures that only showed up sporadically (1 out of over 10 runs) and/or caused the test not to exit.
