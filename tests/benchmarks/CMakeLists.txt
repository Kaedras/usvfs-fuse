cmake_minimum_required(VERSION 3.31)

find_package(benchmark REQUIRED)

add_executable(
        usvfs-benchmarks
        benchmark_utils.cpp
        benchmark_utils.h
        constants.h
        filetree.cpp
        usvfs.cpp
        usvfsmanager.cpp
        utils.cpp
)
set_target_properties(usvfs-benchmarks PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        usvfs-benchmarks
        PRIVATE
        benchmark::benchmark
        benchmark::benchmark_main
        usvfs-fuse
)
target_compile_options(usvfs-benchmarks PRIVATE -Wall -Wextra -Wpedantic)
