cmake_minimum_required (VERSION 3.14)
set(Project minilogger)
project(${Project} C CXX)
set(SOURCE_DIR ${${Project}_SOURCE_DIR})

# check 3rd dependencies for github workflow
set(
    CMAKE_PREFIX_PATH
    "C:/vcpkg/packages/zlib"
    "C:/vcpkg/packages"
    "C:/vcpkg"
    ${CMAKE_PREFIX_PATH}
)

message(STATUS "Project = ${Project}")
message(STATUS "SOURCE_DIR = ${SOURCE_DIR}")
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "COVERAGE = ${COVERAGE}")
message(STATUS "CMAKE_PREFIX_PATH = ${CMAKE_PREFIX_PATH}")

# prepare 3rd libs
set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY FALSE)
find_package(ZLIB 1.1.2)
if (ZLIB_FOUND)
    message(STATUS "using local ZLIB, ZLIB_DIR=${ZLIB_DIR}")
else()
    message(STATUS "no local ZLIB found, ready to download and compile locally")
    include(FetchContent)
    FetchContent_Declare(
        ZLIB
        URL https://github.com/madler/zlib/archive/15c45adb76e81a7e3a8a9e17b2a56eb90f668f44.zip
    )
    FetchContent_MakeAvailable(ZLIB)
    message(STATUS "ZLIB_SOURCE_DIR = ${ZLIB_SOURCE_DIR}")
endif()

find_package(libzip)
if (libzip_FOUND)
    message(STATUS "local libzip found, libzip_DIR = ${libzip_DIR}")
else()
    message(STATUS "no local libzip found, ready to download and compile locally")
    include(FetchContent)
    FetchContent_Declare(
        libzip
        URL https://github.com/nih-at/libzip/archive/0b6ebe6fad8adb1ec95bb5529c5af2185d40c2cf.zip
    )
    FetchContent_MakeAvailable(libzip)
    message(STATUS "libzip_SOURCE_DIR = ${libzip_SOURCE_DIR}")
endif()

# supress MSVC/GCC warnings
if(${CMAKE_HOST_WIN32})
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
else()
    add_compile_options(-Wno-format-security)
endif()

# define headers and sources
set(Headers Logger.h)
set(Sources Logger.cpp)

# build a dynamic library
set(MINILOGGER_DYNAMIC_LIBRARY_TARGET ${Project})
message("Build minilogger dynamic library ${MINILOGGER_DYNAMIC_LIBRARY_TARGET}")
add_library(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} SHARED ${Sources} ${Headers})
set_property(TARGET ${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 11)
# to generate export library when build dynamic library, pass LIBRARY_EXPORT macro
target_compile_definitions(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PRIVATE -DLIBRARY_EXPORT)
target_link_libraries(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} libzip::zip)

# build a static library
set(MINILOGGER_STATIC_LIBRARY_TARGET ${Project}_static)
message("Build minilogger static library ${MINILOGGER_STATIC_LIBRARY_TARGET}")
add_library(${MINILOGGER_STATIC_LIBRARY_TARGET} STATIC ${Sources} ${Headers})
set_property(TARGET ${MINILOGGER_STATIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 11)
target_link_libraries(${MINILOGGER_STATIC_LIBRARY_TARGET} libzip::zip)

# set -DCMAKE_BUILD_TYPE=Debug to enable LLT, set -DCOVERAGE=ON to enable code coverage
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # these config must be put at the level of source code in order to append compile flags
    if ("${COVERAGE}" STREQUAL "lcov" OR "${COVERAGE}" STREQUAL "gcovr")
        message("code coverage is enabled, COVERAGE = ${COVERAGE}, append coverage compiler flags")
        include(${SOURCE_DIR}/test/CodeCoverage.cmake)
        APPEND_COVERAGE_COMPILER_FLAGS()
    endif()

    enable_testing()
    add_subdirectory("test")
endif()
