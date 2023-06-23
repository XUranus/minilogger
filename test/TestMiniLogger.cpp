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

TEST(LoggerTest, BasicLog)
{
    const char* str = "word";
    INFOLOG("hello %s", str);
}
