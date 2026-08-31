cmake_minimum_required(VERSION 3.31)

find_package(benchmark REQUIRED)

add_executable(
        usvfs-benchmarks
        usvfs.cpp
        utils.cpp
        filetree.cpp
        benchmark_utils.h
        benchmark_utils.cpp
        usvfsmanager.cpp
        constants.h
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
