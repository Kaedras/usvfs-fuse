cmake_minimum_required(VERSION 3.28)

find_package(PkgConfig)
pkg_check_modules(FUSE3 REQUIRED IMPORTED_TARGET fuse3)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(spdlog CONFIG REQUIRED)

add_library(usvfs-fuse SHARED)
configure_file(${PROJECT_SOURCE_DIR}/include/usvfs-fuse/usvfs_version.h.in ${PROJECT_SOURCE_DIR}/include/usvfs-fuse/usvfs_version.h)
target_sources(usvfs-fuse
        PRIVATE
            fdmap.cpp
            fdmap.h
            loghelpers.cpp
            loghelpers.h
            mountdata.cpp
            mountdata.h
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
    target_compile_options(usvfs-fuse PRIVATE -fno-omit-frame-pointer -fsanitize=address -fsanitize-recover=address)
endif()

if(ENABLE_UBSAN)
    target_compile_options(usvfs-fuse PRIVATE -fsanitize=undefined)
    target_link_options(usvfs-fuse PRIVATE -fsanitize=undefined)
endif()

target_compile_options(usvfs-fuse PRIVATE -Wall -Wextra -Wpedantic -O2)
target_link_libraries(usvfs-fuse PRIVATE PkgConfig::FUSE3 spdlog::spdlog PUBLIC Qt::Core)
