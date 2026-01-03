cmake_minimum_required(VERSION 3.31)

if (BUILD_TESTING)
    add_subdirectory(unit)
endif()

if (BUILD_PERF_TESTS)
    add_subdirectory(performance)
endif()
