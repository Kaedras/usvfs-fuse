cmake_minimum_required(VERSION 3.28)

if (BUILD_TESTING)
    add_subdirectory(unit)
endif()

if (BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
