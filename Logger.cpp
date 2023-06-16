#include <cstdarg>
#include <unistd.h>

#include "Logger.h"

using namespace xuranus::minilogger;

Logger Logger::instance;

Logger& Logger::GetInstance()
{
    return instance;
}

Logger::Logger()
{}

Logger::~Logger()
{}