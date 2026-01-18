cmake_minimum_required(VERSION 3.31)

find_package(benchmark REQUIRED)

add_executable(
        usvfs-performance-tests
        usvfs.cpp
        utils.cpp
        filetree.cpp
        benchmark_utils.h
)
set_target_properties(usvfs-performance-tests PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        usvfs-performance-tests
        PRIVATE
        benchmark::benchmark
        benchmark::benchmark_main
        usvfs-fuse
)
target_compile_options(usvfs-performance-tests PRIVATE -Wall -Wextra -Wpedantic)
