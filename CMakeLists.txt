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

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/usvfs_fuse-config.cmake"
        INSTALL_DESTINATION "lib/cmake/usvfs_fuse"
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(TARGETS usvfs_fuse EXPORT usvfs_fuse-targets FILE_SET HEADERS)
install(EXPORT usvfs_fuse-targets
        FILE usvfs_fuse-targets.cmake
        DESTINATION lib/cmake/usvfs_fuse
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/usvfs_fuse-config.cmake
        DESTINATION lib/cmake/usvfs_fuse
)
