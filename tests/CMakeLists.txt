cmake_minimum_required(VERSION 4.0)

if (BUILD_UNIT_TESTS OR BUILD_TESTS)
    add_subdirectory(unit)
endif()

if (BUILD_PERF_TESTS OR BUILD_TESTS)
    add_subdirectory(performance)
endif()
