CMAKE_MINIMUM_REQUIRED (VERSION 3.8)
SET(CMAKE_BUILD_TYPE Debug)
PROJECT ("minilogger")

ADD_LIBRARY (minilogger
    SHARED
    "Logger.cpp"
)

ADD_LIBRARY (minilogger_static
    STATIC
    "Logger.cpp"
)

TARGET_LINK_LIBRARIES(minilogger)