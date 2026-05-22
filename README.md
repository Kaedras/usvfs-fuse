# USVFS-FUSE

USVFS-FUSE is an experimental reimplementation of [USVFS](https://github.com/ModOrganizer2/usvfs)
using [libfuse](https://github.com/libfuse/libfuse) for use on Linux.

## Requirements

- CMake >=3.31
- GCC >=14.2
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

optional build options:

- BUILD_TESTING=ON/OFF: build unit tests
- BUILD_BENCHMARKS=ON/OFF: build benchmarks

## Known issues/limitations

- Some functions haven’t been implemented yet (they may not even be required)
- There may be much room for performance optimization
- Unit tests need to be expanded
- Mounting in a separate user namespace by calling `UsvfsManager::setUseMountNamespace(true)` may require an AppArmor
  rule
