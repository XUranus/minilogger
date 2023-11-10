cmake_minimum_required(VERSION 3.14)
set(Project "minilogger_test")
set(${Project} C CXX)

set(Headers)
set(Sources TestMiniLogger.cpp)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# build test and run code coverage
enable_testing()

# auto fetch googletest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(${Project} ${Sources} ${Headers})

target_link_libraries(${Project} PUBLIC 
    minilogger_static
    GTest::gtest_main
)

add_test(
    NAME ${Project}
    COMMAND ${Project}
)

include(GoogleTest)
gtest_discover_tests(${Project})

# configure code coverage
if ("${COVERAGE}" STREQUAL "lcov")
    message("code coverage is enabled, using lcov")
    include(CodeCoverage.cmake)
    APPEND_COVERAGE_COMPILER_FLAGS()
    SETUP_TARGET_FOR_COVERAGE_LCOV(
        NAME "minilogger_coverage_test"
        EXECUTABLE make test
        EXCLUDE "${PROJECT_SOURCE_DIR}/test/*" "/usr/include/*" "*googlemock*" "*googletest*"
    )
elseif ("${COVERAGE}" STREQUAL "gcovr")
    message("code coverage is enabled, using gcovr")
    include(CodeCoverage.cmake)
    APPEND_COVERAGE_COMPILER_FLAGS()
    SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(
        NAME "minilogger_coverage_test"
        EXECUTABLE make test
        BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
        EXCLUDE "${PROJECT_SOURCE_DIR}/test/*" "/usr/include/*" "*googlemock*" "*googletest*" "*build*"
    )
endif()