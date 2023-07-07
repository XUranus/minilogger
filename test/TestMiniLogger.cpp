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
#include "../Logger.h"

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        using namespace xuranus::minilogger;
        LoggerConfig conf {};
        conf.target = LoggerTarget::FILE;
        conf.archiveFilesNumMax = 10;
        conf.fileName = "demo.log";
        conf.logDirPath = "/mnt/c/Users/王星校/Desktop/minilogger/wslbuild";
        if (!Logger::GetInstance()->Init(conf)) {
            std::cerr << "Init logger failed" << std::endl;
        }
    }

    void TearDown() override {
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
}

TEST_F(LoggerTest, LoggerGuard)
{
    INFOLOG_GUARD;
    DBGLOG("line1");
    ERRLOG("line2");
}
