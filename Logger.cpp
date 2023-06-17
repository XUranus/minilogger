#include <cstdarg>
#include <cstdio>
#include <unistd.h>
#include <thread>
#include "Logger.h"

using namespace xuranus::minilogger;

namespace {
    const uint32_t LOGGER_LEVEL_COUNT = 5;
    const uint32_t LOGGER_BUFFER_MAX_LEN = LOGGER_MESSAGE_BUFFER_MAX_LEN + LOGGER_FUNCTION_BUFFER_MAX_LEN + 1024;
}

Logger Logger::instance;

const char* g_loggerLevelStr[LOGGER_LEVEL_COUNT] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

Logger& Logger::GetInstance()
{
    return instance;
}

void Logger::SetLogLevel(LoggerLevel level)
{
    m_level = level;
}

Logger::Logger()
{}

Logger::~Logger()
{}

static std::string ParseDateTime(uint64_t timestamp)
{
    auto millsec = std::chrono::seconds(timestamp);
    auto tp = std::chrono::time_point<
    std::chrono::system_clock, std::chrono::seconds>(millsec);
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm* now = std::gmtime(&tt);
    char strtime[100] = "";
    if (std::strftime(strtime, sizeof(strtime), "%Y-%m-%d %H:%M:%S", now) > 0) {
        return std::string(strtime);
    }
    return std::to_string(timestamp);
}

static std::string FormatFunction(const char* functionMacroLiteral)
{
    std::string function = functionMacroLiteral;
    if (function.length() >= LOGGER_FUNCTION_BUFFER_MAX_LEN) {
        function = function.substr(0, LOGGER_FUNCTION_BUFFER_MAX_LEN);
    }
    return function;
}

void Logger::KeepLog(
    LoggerLevel     level,
    const char*     function,
    uint32_t        line,
    const char*     message,
    uint64_t        timestamp)
{
    char buffer[LOGGER_BUFFER_MAX_LEN] = { '\0' };
    std::thread::id threadID = std::this_thread::get_id();
    std::string datetime = ParseDateTime(timestamp);
    const char* levelStr = g_loggerLevelStr[static_cast<uint32_t>(level)];
    std::string prettyFunction = FormatFunction(function);
    // [datetime][level][message][function:line][threadID]
    const char* format = "[%s][%s][%s][%s:%u][%lu]";
    if (!::snprintf(buffer, LOGGER_BUFFER_MAX_LEN, format,
            datetime.c_str(), levelStr, message, prettyFunction.c_str(), line, threadID)) {
        return;
    }
    std::printf("%s", buffer);
}

