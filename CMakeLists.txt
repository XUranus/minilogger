CMAKE_MINIMUM_REQUIRED (VERSION 3.8)
SET(Project minilogger)
PROJECT(${Project} C CXX)
SET(SOURCE_DIR ${${Project}_SOURCE_DIR})

IF (NOT ${CMAKE_HOST_WIN32})
    ADD_COMPILE_OPTIONS(-Wno-format-security)
ENDIF()

ADD_DEFINITIONS(-DLIBRARY_EXPORT)
ADD_LIBRARY (minilogger
    SHARED
    "Logger.cpp"
)

ADD_LIBRARY (minilogger_static
    STATIC
    "Logger.cpp"
)

TARGET_LINK_LIBRARIES(minilogger)

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
