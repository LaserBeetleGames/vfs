cmake_minimum_required(VERSION 3.21)
project(vfs LANGUAGES C CXX)

# Builds the vfs static library, and zlib with it from the src/zlib submodule unless a zlib target exists
# already. Built on its own, it also builds the tests:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   ctest --test-dir build --build-config Release
#
# In another project: add_subdirectory(vfs), then target_link_libraries(<target> PRIVATE vfs).

option(VFS_BUILD_TESTS "Build the vfs tests" ${PROJECT_IS_TOP_LEVEL})

# zlib, without its gz* file functions
if (NOT TARGET zlib)
    set(VFS_ZLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/zlib)
    if (NOT EXISTS "${VFS_ZLIB_DIR}/zlib.h")
        message(FATAL_ERROR "src/zlib is empty; run: git submodule update --init --recursive")
    endif()
    add_library(zlib STATIC
        ${VFS_ZLIB_DIR}/adler32.c ${VFS_ZLIB_DIR}/compress.c ${VFS_ZLIB_DIR}/crc32.c ${VFS_ZLIB_DIR}/deflate.c
        ${VFS_ZLIB_DIR}/infback.c ${VFS_ZLIB_DIR}/inffast.c ${VFS_ZLIB_DIR}/inflate.c ${VFS_ZLIB_DIR}/inftrees.c
        ${VFS_ZLIB_DIR}/trees.c ${VFS_ZLIB_DIR}/uncompr.c ${VFS_ZLIB_DIR}/zutil.c
    )
    target_include_directories(zlib SYSTEM PUBLIC ${VFS_ZLIB_DIR})
endif()

add_library(vfs STATIC
    src/ArchiveFileSystem.cpp
    src/Compression.cpp
    src/FileStream.cpp
    src/FolderFileSystem.cpp
    src/VirtualFileSystem.cpp
)
target_include_directories(vfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(vfs PUBLIC cxx_std_20)
target_link_libraries(vfs PRIVATE zlib)

# built on its own, a cast in C style or an override without the keyword fails the build
if (PROJECT_IS_TOP_LEVEL)
    if (MSVC)
        target_compile_options(vfs PRIVATE /W4)
    else()
        target_compile_options(vfs PRIVATE -Wall -Wextra -Werror=old-style-cast -Werror=suggest-override)
    endif()
endif()

if (VFS_BUILD_TESTS)
    enable_testing()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/doctest/doctest/doctest.h")
        message(FATAL_ERROR "tests/doctest is empty; run: git submodule update --init --recursive")
    endif()

    add_executable(test_vfs tests/test_vfs.cpp)
    target_include_directories(test_vfs SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/doctest)
    target_link_libraries(test_vfs PRIVATE vfs)
    target_compile_definitions(test_vfs PRIVATE VFS_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    if (MSVC)
        target_compile_options(test_vfs PRIVATE /W4 /EHsc)
    else()
        target_compile_options(test_vfs PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME test_vfs COMMAND test_vfs)

    find_program(VFS_NODE node)
    if (VFS_NODE)
        add_test(NAME test_pack COMMAND ${VFS_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test/pack.test.js)
    endif()
endif()
