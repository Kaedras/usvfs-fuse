cmake_minimum_required(VERSION 3.28)

find_package(PkgConfig)
pkg_check_modules(FUSE3 REQUIRED IMPORTED_TARGET fuse3)

find_package(ICU REQUIRED COMPONENTS data uc)
find_package(spdlog CONFIG REQUIRED)

add_library(usvfs-fuse SHARED)
target_precompile_headers(usvfs-fuse PRIVATE pch.h)
target_sources(usvfs-fuse
        PRIVATE
            fdmap.cpp
            fdmap.h
            logger.h
            loghelpers.cpp
            loghelpers.h
            mountstate.cpp
            mountstate.h
            usvfs.cpp
            usvfs.h
            usvfsmanager.cpp
            utils.cpp
            utils.h
            virtualfiletreeitem.cpp
            virtualfiletreeitem.h
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/usvfs-fuse/logging.h
            ${PROJECT_SOURCE_DIR}/include/usvfs-fuse/usvfs_version.h
            ${PROJECT_SOURCE_DIR}/include/usvfs-fuse/usvfsmanager.h
)

if(ENABLE_ASAN)
    target_link_libraries(usvfs-fuse PUBLIC asan)
    target_compile_options(usvfs-fuse PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

target_compile_options(usvfs-fuse PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(usvfs-fuse PRIVATE PkgConfig::FUSE3 spdlog::spdlog ICU::data ICU::uc)

# from https://stackoverflow.com/a/78987613
# NOTE: The dbg extension is used solely because it is used as a standard
# NOTE: In Linux documentation etc.
# NOTE: Obtained from https://www.man7.org/linux/man-pages/man1/objcopy.1.html
add_custom_command(
        TARGET usvfs-fuse
        # Separate the Debug Information from the Target file
        COMMAND ${CMAKE_OBJCOPY} ARGS --only-keep-debug $<TARGET_FILE:usvfs-fuse> $<TARGET_FILE:usvfs-fuse>.dbg
        # As we have previously separated the debug information from the
        # previous output, remove all debug information from the same
        COMMAND ${CMAKE_OBJCOPY} ARGS --strip-debug $<TARGET_FILE:usvfs-fuse>
        # Link the Separate Debug Information with the Final Release
        COMMAND ${CMAKE_OBJCOPY} ARGS --add-gnu-debuglink=$<TARGET_FILE:usvfs-fuse>.dbg $<TARGET_FILE:usvfs-fuse>
        POST_BUILD
)
# NOTE: Add this to the clean target
# NOTE: This is needed because otherwise old debug information
# NOTE:  Will not be cleared upon clean
set_property(
        TARGET usvfs-fuse
        APPEND
        PROPERTY
        ADDITIONAL_CLEAN_FILES
        $<TARGET_FILE:usvfs-fuse>.dbg
)
install(FILES $<TARGET_FILE:usvfs-fuse>.dbg DESTINATION debug)
