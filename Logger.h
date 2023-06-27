#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <mutex>

// define library export macro
#ifdef _WIN32
    #ifdef LIBRARY_EXPORT
        #define MINILOGGER_API __declspec(dllexport)
    #else
        #define MINILOGGER_API __declspec(dllimport)
    #endif
#else
    #define MINILOGGER_API  __attribute__((__visibility__("default")))
#endif

#ifdef _MSC_VER
    #define MINI_LOGGER_FUNCTION __FUNCSIG__
#endif
#ifdef __linux__
    #define MINI_LOGGER_FUNCTION __PRETTY_FUNCTION__
#endif

#define MINI_LOGGER_NAMESPACE   ::xuranus::minilogger
#define MINI_LOGGER_LOG_FUN     MINI_LOGGER_NAMESPACE::Logger::GetInstance().Log


#ifdef __linux__
#define DBGLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, ##args)

#define INFOLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, ##args)    

#define WARNLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING, MINI_LOGGER_FUNCTION, __LINE__, format, ##args)

#define ERRLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::ERROR, MINI_LOGGER_FUNCTION, __LINE__, format, ##args)
#endif

// reference: https://learn.microsoft.com/en-us/cpp/preprocessor/variadic-macros?view=msvc-170
#ifdef _MSC_VER
#define DBGLOG(format, ...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, __VA_ARGS__)

#define INFOLOG(format, ...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, __VA_ARGS__)

#define WARNLOG(format, ...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING, MINI_LOGGER_FUNCTION, __LINE__, format, __VA_ARGS__)

#define ERRLOG(format, ...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::ERROR, MINI_LOGGER_FUNCTION, __LINE__, format, __VA_ARGS__)
#endif

#define DBGLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__)

#define INFOLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::INFO, MINI_LOGGER_FUNCTION, __LINE__)

#define WARNLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING, MINI_LOGGER_FUNCTION, __LINE__)

namespace xuranus {
namespace minilogger {

const int LOGGER_MESSAGE_BUFFER_MAX_LEN = 4096;
const int LOGGER_FUNCTION_BUFFER_MAX_LEN = 1024;

enum class MINILOGGER_API LoggerLevel {
    DEBUG       = 0,
    INFO        = 1,
    WARNING     = 2,
    ERROR       = 3,
    FATAL       = 4
};

enum class MINILOGGER_API LoggerTarget {
    STDOUT = 1,
    FILE   = 2
};

class MINILOGGER_API Logger {
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
    LoggerLevel     m_level { LoggerLevel::DEBUG };
};

class LoggerGuard {
public:
    LoggerGuard(LoggerLevel level, const char* function, uint32_t line);
    ~LoggerGuard();
private:
    LoggerLevel     m_level;
    const char*     m_function;
    uint32_t        m_line;
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
        std::strncpy(messageBuffer, format, sizeof(messageBuffer) - 1);
        KeepLog(level, function, line, messageBuffer, timestamp);
        return;
    }
    if (::snprintf(messageBuffer, LOGGER_MESSAGE_BUFFER_MAX_LEN, format, args...) < 0) {
        std::fill(messageBuffer, messageBuffer + LOGGER_MESSAGE_BUFFER_MAX_LEN, 0);
        std::strncpy(messageBuffer, "...", sizeof(messageBuffer) - 1);
    }
    KeepLog(level, function, line, messageBuffer, timestamp);
}

}
}

#endif
