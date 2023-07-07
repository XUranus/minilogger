CMAKE_MINIMUM_REQUIRED (VERSION 3.8)
SET(Project minilogger)
PROJECT(${Project} C CXX)
SET(SOURCE_DIR ${${Project}_SOURCE_DIR})

IF (NOT ${CMAKE_HOST_WIN32})
    ADD_COMPILE_OPTIONS(-Wno-format-security)
ENDIF()

IF(${CMAKE_HOST_WIN32})
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
ENDIF()

SET(Headers Logger.h)
SET(Sources Logger.cpp)

SET(MINILOGGER_DYNAMIC_LIBRARY_TARGET ${Project})
SET(MINILOGGER_STATIC_LIBRARY_TARGET ${Project}_static)

MESSAGE("Build dynamic library ${MINILOGGER_DYNAMIC_LIBRARY_TARGET}")
MESSAGE("Build static library ${MINILOGGER_STATIC_LIBRARY_TARGET}")
ADD_LIBRARY(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} SHARED ${Sources} ${Headers})
ADD_LIBRARY(${MINILOGGER_STATIC_LIBRARY_TARGET}  STATIC ${Sources} ${Headers})

SET_PROPERTY(TARGET ${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 17)
SET_PROPERTY(TARGET ${MINILOGGER_STATIC_LIBRARY_TARGET} PROPERTY CXX_STANDARD 17)

# to generate export library when build dynamic library
TARGET_COMPILE_DEFINITIONS(${MINILOGGER_DYNAMIC_LIBRARY_TARGET} PRIVATE -DLIBRARY_EXPORT)

MESSAGE("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
MESSAGE("SOURCE_DIR is ${SOURCE_DIR}")

# build test and run code coverage
IF (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Coverage")

    ENABLE_TESTING()
    ADD_SUBDIRECTORY("test")

    IF (CMAKE_BUILD_TYPE STREQUAL "Coverage")

        INCLUDE(${SOURCE_DIR}/test/CodeCoverage.cmake)
        APPEND_COVERAGE_COMPILER_FLAGS()
        SETUP_TARGET_FOR_COVERAGE_LCOV(
            NAME "minilogger_lcov_test"
            EXECUTABLE make test
            EXCLUDE "${PROJECT_SOURCE_DIR}/test/*" "/usr/include/*" "*googlemock*" "*googletest*"
        )
    
        SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(
            NAME "minilogger_gcovr_test"
            EXECUTABLE make test
            BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
            EXCLUDE "${PROJECT_SOURCE_DIR}/test/*" "/usr/include/*" "*googlemock*" "*googletest*" "*build*"
        )
    ENDIF()
ENDIF()
