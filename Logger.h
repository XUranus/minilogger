#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <mutex>

namespace xuranus {
namespace minilogger {

const int LOGGER_MESSAGE_BUFFER_MAX_LEN = 4096;
const int LOGGER_FUNCTION_BUFFER_MAX_LEN = 1024;

enum class LoggerLevel {
    DEBUG       = 0,
    INFO        = 1,
    WARNING     = 2,
    ERROR       = 3,
    FATAL       = 4
};

#ifdef __linux__
#define DBGLOG(format, args...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::DEBUG, __PRETTY_FUNCTION__, __LINE__ ,format, ##args)

#define INFOLOG(format, args...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::DEBUG, __PRETTY_FUNCTION__, __LINE__ ,format, ##args)    

#define WARNLOG(format, args...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::WARNING, __PRETTY_FUNCTION__, __LINE__, format, ##args)

#define ERRLOG(format, args...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::ERROR, __PRETTY_FUNCTION__, __LINE__, format, ##args)
#endif

// reference: https://learn.microsoft.com/en-us/cpp/preprocessor/variadic-macros?view=msvc-170
#ifdef _MSC_VER
#define DBGLOG(format, ...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::DEBUG, __FUNCSIG__, __LINE__ ,format, __VA_ARGS__)

#define INFOLOG(format, ...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::DEBUG, __FUNCSIG__, __LINE__ ,format, __VA_ARGS__)

#define WARNLOG(format, ...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::WARNING, __FUNCSIG__, __LINE__, format, __VA_ARGS__)

#define ERRLOG(format, ...) \
    ::xuranus::minilogger::Logger::GetInstance().Log(::xuranus::minilogger::LoggerLevel::ERROR, __FUNCSIG__, __LINE__, format, __VA_ARGS__)
#endif

enum class LoggerTarget {
    STDOUT = 1,
    FILE   = 2
};

class Logger {
public:
    static Logger& GetInstance();

    template<class... Args>
    void Log(LoggerLevel level, const char* function, uint32_t line, const char* format, Args...);
    void SetLogLevel(LoggerLevel level);
    ~Logger();
private:
    void KeepLog(LoggerLevel level, const char* function, uint32_t line, const char* message, uint64_t timestamp);
    Logger();

public:
    static Logger   instance;
private:
    std::mutex      m_mutex;
    LoggerLevel     m_level {LoggerLevel::DEBUG};
};

// format
template<class... Args>
void Logger::Log(
    LoggerLevel     level,
    const char*     function,
    uint32_t        line,
    const char*     format,
    Args...         args)
{
    using clock = std::chrono::system_clock;
    if (m_level > level) {
        return;
    }
    uint64_t timestamp = clock::now().time_since_epoch().count() / clock::period::den;
    char messageBuffer[LOGGER_MESSAGE_BUFFER_MAX_LEN] = { '\0' };
    if (sizeof...(args) == 0) { // empty args optimization
        strcpy(messageBuffer, format);
        KeepLog(level, function, line, messageBuffer, timestamp);
        return;
    }
    if (::snprintf(messageBuffer, LOGGER_MESSAGE_BUFFER_MAX_LEN, format, args...) < 0) {
        memset(messageBuffer, '\0', LOGGER_MESSAGE_BUFFER_MAX_LEN);
        strcpy(messageBuffer, "...");
    }
    KeepLog(level, function, line, messageBuffer, timestamp);
}

}
}

#endif
