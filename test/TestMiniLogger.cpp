/*================================================================
*   Copyright (C) 2023 XUranus All rights reserved.
*   
*   File:         TestMiniLogger.cpp
*   Author:       XUranus
*   Date:         2023-06-23
*   Description:  
*
================================================================*/

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include "../Logger.h"

namespace {
    const std::string LOGGER_FILE_NAME = "demo.log";
}

class LoggerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        using namespace xuranus::minilogger;
        LoggerConfig conf {};
        conf.target = LoggerTarget::FILE;
        conf.archiveFilesNumMax = 10;
        conf.fileName = LOGGER_FILE_NAME;

        // get current path
        char currentDir[FILENAME_MAX];
        if (GetCurrentDir(currentDir, sizeof(currentDir)) != nullptr) {
            std::cout << "Current Directory: " << currentDir << std::endl;
        } else {
            std::cerr << "Failed to get current directory." << std::endl;
        }
        
        conf.logDirPath = currentDir;
        std::cout << "using logger path: " << conf.logDirPath << ", name: " << conf.fileName << std::endl;
        Logger::GetInstance()->SetLogLevel(LoggerLevel::DEBUG);
        if (!Logger::GetInstance()->Init(conf)) {
            std::cerr << "Init logger failed" << std::endl;
        }
    }

    static void TearDownTestCase() {
        std::cout << "TearDown" << std::endl;
        using namespace xuranus::minilogger;
        Logger::GetInstance()->Destroy();
    }
};

TEST_F(LoggerTest, BasicLogger)
{
    const char* str = "word";
    int iv = 10;
    INFOLOG("hello %s", str);
    ERRLOG("hello world");
    DBGLOG("int value = %d", iv);
    uint64_t amount = 500000;
    MINI_LOG(LINFO)
        << "MiniLogger is a asynchronize logger that can write "
        << amount
        << " logs per second"
        << LOGENDL;
}

TEST_F(LoggerTest, LoggerGuard)
{
    INFOLOG_GUARD;
    DBGLOG("line1");
    ERRLOG("line2");
}

static void LogProducerThread(int lineCounter)
{
    const char* str1 = "hello world hello world";
    const char* str2 = "hello world hello world";
    const char* str3 = "hello world hello world";
    const int v1 = 114514;
    const uint64_t v2 = 1919810;
    // set thread key
    static std::hash<std::thread::id> hasher;
    uint64_t threadIdUint64 = hasher(std::this_thread::get_id());
    const std::string key = std::string("Key_") + std::to_string(threadIdUint64);
    xuranus::minilogger::Logger::GetInstance()->SetThreadLocalKey(key);
    // start to log
    for (int i = 0; i < lineCounter; i++) {
        INFOLOG("this is a example log for test performance, str1 = %s, str2 = %s, str3 = %s, int1 = %d, int2 = %lu",
            str1, str2, str3, v1, v2);
    }
}

static void RunPerformanceTest(int threadNum, int lines) {
    std::vector<std::thread> threads;
    for (int i = 0; i < threadNum; i++) {
        threads.emplace_back(LogProducerThread, lines);
    }
    for (std::thread& t: threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

TEST_F(LoggerTest, Performance)
{
    std::cout << "============ Performance Test for 1M logs ============" << std::endl;
    std::cout << "threads\t\t line\t\t duration(milliseconds)" << std::endl;
    uint64_t totalDuration = 0;
    uint64_t totalLogs = 0;
    const int batchTotalLogs = 1000000;
    for (int threadNum : std::vector<int>{1000, 100, 10, 1}) {
        auto t1 = std::chrono::system_clock::now();
        RunPerformanceTest(threadNum, batchTotalLogs / threadNum);
        auto t2 = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        totalDuration += duration;
        totalLogs += batchTotalLogs;
        std::cout 
            << threadNum << "\t\t"
            << batchTotalLogs / threadNum << "\t\t"
            << duration << std::endl;
    }
    std::cout << std::endl;
    std::cout << totalLogs / totalDuration << "k logs per seconds on average" << std::endl;
}
