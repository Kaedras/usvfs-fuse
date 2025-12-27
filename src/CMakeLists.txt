cmake_minimum_required(VERSION 4.0)

find_package(PkgConfig)
pkg_check_modules(FUSE3 REQUIRED IMPORTED_TARGET fuse3)

find_package(ICU REQUIRED COMPONENTS data uc)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(spdlog REQUIRED)

add_library(usvfs_fuse SHARED)
target_precompile_headers(usvfs_fuse PRIVATE pch.h)
target_sources(usvfs_fuse
        PRIVATE
            fdmap.cpp
            mountstate.cpp
            usvfs.cpp
            usvfsmanager.cpp
            utils.cpp
            virtualfiletreeitem.cpp
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/fdmap.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/logger.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/mountstate.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/usvfs.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/utils.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/virtualfiletreeitem.h
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/usvfs_version.h
            ${PROJECT_SOURCE_DIR}/include/usvfs_fuse/usvfsmanager.h
)

target_compile_options(usvfs_fuse PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(usvfs_fuse PUBLIC PkgConfig::FUSE3 spdlog::spdlog ICU::data ICU::uc Qt::Core)
