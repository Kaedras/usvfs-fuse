cmake_minimum_required(VERSION 3.31)

include(CMakePackageConfigHelpers)

project(usvfs-fuse)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(src)

set(BUILD_TESTING ${BUILD_TESTING} CACHE BOOL "build unit tests for usvfs-fuse")
set(BUILD_PERF_TESTS ${BUILD_PERF_TESTS} CACHE BOOL "build performance tests for usvfs-fuse")

if (BUILD_TESTING OR BUILD_PERF_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/usvfs-fuseConfig.cmake"
        INSTALL_DESTINATION "lib/cmake/usvfs-fuse"
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(TARGETS usvfs-fuse EXPORT usvfs-fuseTargets FILE_SET HEADERS)
install(EXPORT usvfs-fuseTargets
        FILE usvfs-fuseTargets.cmake
        DESTINATION lib/cmake/usvfs-fuse
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/usvfs-fuseConfig.cmake
        DESTINATION lib/cmake/usvfs-fuse
)
