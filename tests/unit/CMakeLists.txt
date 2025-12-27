cmake_minimum_required(VERSION 4.0)

find_package(GTest REQUIRED)

add_executable(
        usvfs-tests
        filetree.cpp
        usvfs.cpp
        utils.cpp
)
set_target_properties(usvfs-tests PROPERTIES CXX_STANDARD 23)
target_link_libraries(
        usvfs-tests
        PRIVATE
        GTest::gtest_main
        usvfs-fuse
)
target_compile_options(usvfs-tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(usvfs-tests)
