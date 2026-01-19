cmake_minimum_required(VERSION 3.31)

find_package(GTest REQUIRED)

add_executable(
        usvfs-tests
        filetree.cpp
        usvfs.cpp
        utils.cpp
)
set_target_properties(usvfs-tests PROPERTIES CXX_STANDARD 23)

if(ENABLE_ASAN)
    target_link_libraries(usvfs-tests PUBLIC asan)
    target_compile_options(usvfs-tests PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

target_link_libraries(
        usvfs-tests
        PRIVATE
        GTest::gtest_main
        usvfs-fuse
)
target_compile_options(usvfs-tests PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(usvfs-tests)
