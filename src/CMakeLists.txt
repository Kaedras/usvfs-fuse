cmake_minimum_required(VERSION 3.31)

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

target_compile_options(usvfs-fuse PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(usvfs-fuse PRIVATE PkgConfig::FUSE3 spdlog::spdlog ICU::data ICU::uc)
