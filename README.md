# MiniLogger
C++ Logger implement

## Feature & TODO
 - [X] Double Buffering & Asynchronized Writting
 - [ ] Record Stacktrace & Dump File From Crash
 - [X] Configurable Congestion Policy
 - [ ] Auto Compressing & Archiving

## Require
 - CXX17
 - MSVC/GCC

## Usage
```cpp
int main()
{
    // init logger
    using namespace xuranus::minilogger;
    LoggerConfig conf {};
    conf.target = LoggerTarget::FILE;
    conf.fileName = "demo.log";
    conf.logDirPath = "/tmp";
    Logger::GetInstance()->SetLogLevel(LoggerLevel::DEBUG);
    if (!Logger::GetInstance()->Init(conf)) {
        std::cerr << "Init logger failed" << std::endl;
        return 1;
    }

    // keep log
    const char* str = "word";
    int iv = 10;
    INFOLOG("hello %s", str);
    ERRLOG("hello world");
    DBGLOG("int value = %d", iv);

    // destory logger
    Logger::GetInstance()->Destroy();
    return;
}
```

## Build & Test
build static/dynamic library:
```
mkdir build && cd build
cmake .. && cmake --build .
```

build test coverage:
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Coverage && cmake --build .
make minilogger_lcov_test # lcov
make minilogger_gcovr_test # gcovr
```

## Performance
Testing 1 million line of logs, archiving a throughput of 0.5 million lines of log per second.

| threads | each thread log | duration(sec) |
|---------|-----------------|---------------|
| 1000    | 1000            |       1.44      |
| 100     | 10000           |       1.42    |
| 10      | 100000          |       1.06    |
| 1       | 1000000         |       3.87    |

