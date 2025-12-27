cmake_minimum_required(VERSION 4.0)

include(CMakePackageConfigHelpers)

project(usvfs_fuse)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(src)

set(BUILD_UNIT_TESTS ${BUILD_UNIT_TESTS} CACHE BOOL "build unit tests for usvfs-fuse")
set(BUILD_PERF_TESTS ${BUILD_PERF_TESTS} CACHE BOOL "build performance tests for usvfs-fuse")
set(BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "build unit and performance tests for usvfs-fuse")

if (BUILD_UNIT_TESTS OR BUILD_PERF_TESTS OR BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
