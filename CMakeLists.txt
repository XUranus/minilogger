CMAKE_MINIMUM_REQUIRED (VERSION 3.14)
SET(Project minilogger)
PROJECT(${Project} C CXX)
SET(SOURCE_DIR ${${Project}_SOURCE_DIR})

MESSAGE("Project = ${Project}")
MESSAGE("SOURCE_DIR = ${SOURCE_DIR}")
MESSAGE("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
MESSAGE("COVERAGE = ${COVERAGE}")

# supress MSVC/GCC warnings
IF(${CMAKE_HOST_WIN32})
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
ELSE()
    ADD_COMPILE_OPTIONS(-Wno-format-security)
ENDIF()

# define headers and sources
SET(Headers Logger.h)
SET(Sources Logger.cpp)

# build a dynamic library
SET(MINILOGGER_DYNAMIC_LIBRARY_TARGET ${Project})
MESSAGE("Build minilogger dynamic library ${MINILOGGER_DYNAMIC_LIBRARY_TARGET}")
ADD_LIBRARY(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} SHARED ${Sources} ${Headers})
SET_PROPERTY(TARGET ${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 11)
# to generate export library when build dynamic library, pass LIBRARY_EXPORT macro
TARGET_COMPILE_DEFINITIONS(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PRIVATE -DLIBRARY_EXPORT)

# build a static library
SET(MINILOGGER_STATIC_LIBRARY_TARGET ${Project}_static)
MESSAGE("Build minilogger static library ${MINILOGGER_STATIC_LIBRARY_TARGET}")
ADD_LIBRARY(${MINILOGGER_STATIC_LIBRARY_TARGET} STATIC ${Sources} ${Headers})
SET_PROPERTY(TARGET ${MINILOGGER_STATIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 11)

# set -DCMAKE_BUILD_TYPE=Debug to enable LLT, set -DCOVERAGE=ON to enable code coverage
IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # these config must be put at the level of source code in order to append compile flags
    IF (${COVERAGE} STREQUAL "lcov" OR ${COVERAGE} STREQUAL "gcovr")
        MESSAGE("code coverage is enabled, COVERAGE = ${COVERAGE}, append coverage compiler flags")
        INCLUDE(${SOURCE_DIR}/test/CodeCoverage.cmake)
        APPEND_COVERAGE_COMPILER_FLAGS()
    ENDIF()

    ENABLE_TESTING()
    ADD_SUBDIRECTORY("test")
ENDIF()
