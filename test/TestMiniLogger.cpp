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

TEST(LoggerTest, BasicLogger)
{
    const char* str = "word";
    int iv = 10;
    INFOLOG("hello %s", str);
    ERRLOG("hello world");
    DBGLOG("int value = %d", iv);
}

TEST(LoggerTest, LoggerGuard)
{
    INFOLOG_GUARD;
    DBGLOG("line1");
    ERRLOG("line2");
}
