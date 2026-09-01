cmake_minimum_required(VERSION 3.28)

project(usvfs-fuse VERSION 0.11.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(src)

option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_UBSAN "Enable undefined behavior sanitizer" OFF)

set(BUILD_TESTING ${BUILD_TESTING} CACHE BOOL "build unit tests for usvfs-fuse")
set(BUILD_BENCHMARKS ${BUILD_BENCHMARKS} CACHE BOOL "build benchmarks for usvfs-fuse")

if (BUILD_TESTING OR BUILD_BENCHMARKS)
    if(BUILD_TESTING)
        enable_testing()
    endif()
    add_subdirectory(tests)
endif()

set(export_dst_dir "${CMAKE_INSTALL_LIBDIR}/cmake/usvfs-fuse")

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/usvfs-fuseConfig.cmake"
        INSTALL_DESTINATION "${export_dst_dir}"
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(TARGETS usvfs-fuse EXPORT usvfs-fuseTargets FILE_SET HEADERS)
install(EXPORT usvfs-fuseTargets
        FILE usvfs-fuseTargets.cmake
        DESTINATION "${export_dst_dir}"
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/usvfs-fuseConfig.cmake
        DESTINATION "${export_dst_dir}"
)
