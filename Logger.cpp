#include <cstdarg>
#include <cstdio>
#include <thread>
#include <iomanip>
#include <string>
#include <sstream>
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

static std::string ParseDateTime(uint64_t sec)
{
    namespace chrono = std::chrono;
    std::ostringstream oss;
    auto seconds = chrono::duration_cast<chrono::seconds>(chrono::seconds(sec));
    auto timePoint = std::chrono::system_clock::time_point{} + seconds;
    std::time_t timestamp = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* timeinfo = std::localtime(&timestamp);
    if (timeinfo == nullptr) {
        oss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    return "0000-00-00 00:00:00";
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
    const char* format = "[%s][%s][%s][%s:%u][%lu]\n";
    if (!::snprintf(buffer, LOGGER_BUFFER_MAX_LEN, format,
            datetime.c_str(), levelStr, message, prettyFunction.c_str(), line, threadID)) {
        return;
    }
    std::printf("%s", buffer);
}

LoggerGuard::LoggerGuard(LoggerLevel level, const char* function, uint32_t line)
 : m_level(level), m_function(function), m_line(line)
{
    Logger::GetInstance().Log(level, function, line, "Logger Guard, Enter %s", function);
}

LoggerGuard::~LoggerGuard()
{
    Logger::GetInstance().Log(m_level, m_function, m_line, "Logger Guard, Exit %s", m_function);
}