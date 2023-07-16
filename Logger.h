#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>

/*
 *
 * @brief
 * add -DLIBRARY_EXPORT build param to export lib on Win32 MSVC
 * define LIBRARY_IMPORT before including Json.h to add __declspec(dllimport) to use dll library
 * libminilogger is linked static by default
 */

// define library export macro
#ifdef _WIN32
    #ifdef LIBRARY_EXPORT
        #define MINILOGGER_API __declspec(dllexport)
    #else
        #ifdef LIBRARY_IMPORT
            #define MINILOGGER_API __declspec(dllimport)
        #else
            #define MINILOGGER_API
        #endif
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
#define MINI_LOGGER_LOG_FUN     MINI_LOGGER_NAMESPACE::Log

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

const uint64_t LOGGER_MESSAGE_BUFFER_MAX_LEN = 4096;
const uint64_t LOGGER_FUNCTION_BUFFER_MAX_LEN = 1024;
const uint64_t ONE_MB = 1024 * 1024;
const uint64_t LOGGER_BUFFER_SIZE_MAX = 2 * 32 * ONE_MB;
const uint64_t LOGGER_BUFFER_SIZE_DEFAULT = 16 * ONE_MB;

enum class MINILOGGER_API LoggerLevel {
    DEBUG       = 0,
    INFO        = 1,
    WARNING     = 2,
    ERROR       = 3,
    FATAL       = 4
};

enum class MINILOGGER_API LoggerTarget {
    STDOUT      = 1,
    FILE        = 2
};

enum class MINILOGGER_API CongestionControlPolicy {
    BLOCKING    = 1,
    DROPPING    = 2
};

struct LoggerConfig {
    LoggerTarget    target { LoggerTarget::STDOUT };    // output to file or stdout
    std::string     logDirPath;                         // directory path to generate log file
    std::string     fileName;                           // log file name prefix, ${fileName}.log
    uint64_t        fileSizeMax;                        // log file archive threashold in bytes
    uint64_t        archiveFilesNumMax;                 // max num of archive file to keep
    uint64_t        bufferSize { LOGGER_BUFFER_SIZE_DEFAULT }; // logger takes (2 * bufferSize) bytes for buffering
};

class MINILOGGER_API Logger {
public:
    static Logger* GetInstance();
    // use fixed configuration to init logger (only can be called once)
    virtual bool Init(const LoggerConfig& conf) = 0;
    // change configutation that can be modified at runtime
    virtual void SetCongestionControlPolicy(CongestionControlPolicy policy) = 0;
    virtual void SetLogLevel(LoggerLevel level) = 0;
    // must be invoked before application exit
    virtual void Destroy() = 0;

    virtual void KeepLog(LoggerLevel level, const char* function, uint32_t line, const char* message, uint64_t timestamp) = 0;
    virtual bool ShouldKeepLog(LoggerLevel level) const = 0;
    virtual ~Logger();
};

class MINILOGGER_API LoggerGuard {
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
void Log(
    LoggerLevel     level,
    const char*     function,
    uint32_t        line,
    const char*     format,
    Args...         args)
{
    // check current log level
    if (!Logger::GetInstance()->ShouldKeepLog(level)) {
        return;
    }
    namespace chrono = std::chrono;
    using clock = std::chrono::system_clock;
    //uint64_t timestamp = clock::now().time_since_epoch().count() / clock::period::den;
    uint64_t timestamp = chrono::duration_cast<chrono::microseconds>(clock::now().time_since_epoch()).count(); 
    char messageBuffer[LOGGER_MESSAGE_BUFFER_MAX_LEN] = { '\0' };
    if (sizeof...(args) == 0) { // empty args optimization
        std::strncpy(messageBuffer, format, sizeof(messageBuffer) - 1);
        Logger::GetInstance()->KeepLog(level, function, line, messageBuffer, timestamp);
        return;
    }
    if (::snprintf(messageBuffer, LOGGER_MESSAGE_BUFFER_MAX_LEN, format, args...) < 0) {
        std::fill(messageBuffer, messageBuffer + LOGGER_MESSAGE_BUFFER_MAX_LEN, 0);
        std::strncpy(messageBuffer, "...", sizeof(messageBuffer) - 1);
    }
    Logger::GetInstance()->KeepLog(level, function, line, messageBuffer, timestamp);
}

}
}

#endif
