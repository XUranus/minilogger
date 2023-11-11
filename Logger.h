#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <chrono>
#include <sstream>
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
    #define MINI_LOGGER_FUNCTION MINI_LOGGER_NAMESPACE::GetFunctionName(__FUNCSIG__)
#else
    #define MINI_LOGGER_FUNCTION MINI_LOGGER_NAMESPACE::GetFunctionName(__PRETTY_FUNCTION__)
#endif

#define ENABLE_STREAM_LOGGER
#define MINI_LOGGER_NAMESPACE   ::xuranus::minilogger
#define MINI_LOGGER_LOG_FUN     MINI_LOGGER_NAMESPACE::Log

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

#else

#define DBGLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, ##args)

#define INFOLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__ ,format, ##args)    

#define WARNLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING, MINI_LOGGER_FUNCTION, __LINE__, format, ##args)

#define ERRLOG(format, args...) \
    MINI_LOGGER_LOG_FUN(MINI_LOGGER_NAMESPACE::LoggerLevel::ERROR, MINI_LOGGER_FUNCTION, __LINE__, format, ##args)
#endif

#define DBGLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG, MINI_LOGGER_FUNCTION, __LINE__)

#define INFOLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::INFO, MINI_LOGGER_FUNCTION, __LINE__)

#define WARNLOG_GUARD \
    MINI_LOGGER_NAMESPACE::LoggerGuard mini_logger_guard(MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING, MINI_LOGGER_FUNCTION, __LINE__)

#ifdef ENABLE_STREAM_LOGGER
#define MINI_LOG(LOG_LEVEL) MINI_LOGGER_NAMESPACE::LoggerStream(LOG_LEVEL, MINI_LOGGER_FUNCTION, __LINE__)
#define LOGENDL std::endl

#define LDBG    MINI_LOGGER_NAMESPACE::LoggerLevel::DEBUG
#define LINFO   MINI_LOGGER_NAMESPACE::LoggerLevel::INFO
#define LWARN   MINI_LOGGER_NAMESPACE::LoggerLevel::WARNING
#define LERR    MINI_LOGGER_NAMESPACE::LoggerLevel::ERROR
#define LFATAL  MINI_LOGGER_NAMESPACE::LoggerLevel::FATAL
#endif

namespace xuranus {
namespace minilogger {

const uint64_t LOGGER_MESSAGE_BUFFER_MAX_LEN = 4096;
const uint64_t LOGGER_FUNCTION_BUFFER_MAX_LEN = 1024;
const std::size_t ONE_MB = 1024 * 1024;
const std::size_t LOGGER_BUFFER_SIZE_MAX = 2 * 32 * ONE_MB;
const std::size_t LOGGER_BUFFER_SIZE_DEFAULT = 16 * ONE_MB;

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
    LoggerTarget    target { LoggerTarget::STDOUT };           ///> output to file or stdout
    std::string     logDirPath;                                ///> directory path to generate log file
    std::string     fileName;                                  ///> log file name prefix, ${fileName}.log
    std::size_t     fileSizeMax;                               ///> log file archive threashold in bytes
    std::string     archiveFileName;                           ///> archive file name, no extension required
    uint64_t        archiveFilesNumMax;                        ///> max num of archive file to keep
    std::size_t     bufferSize { LOGGER_BUFFER_SIZE_DEFAULT }; ///> logger takes 2 * bufferSize bytes for buffering
};

class MINILOGGER_API Logger {
public:
    static Logger* GetInstance();
    // use fixed configuration to init logger (only can be called once)
    virtual bool Init(const LoggerConfig& conf) = 0;
    // change configutation that can be modified at runtime
    virtual void SetCongestionControlPolicy(CongestionControlPolicy policy) = 0;
    virtual void SetLogLevel(LoggerLevel level) = 0;
    virtual void SetThreadLocalKey(const std::string& key) = 0;
    // must be invoked before application exit
    virtual void Destroy() = 0;

    virtual void KeepLog(LoggerLevel level, const char* function, uint32_t line, const char* message, uint64_t timestamp) = 0;
    virtual bool ShouldKeepLog(LoggerLevel level) const = 0;
    virtual ~Logger();
};

/**
 * @brief Get and format function name at compile time
 */
constexpr const char* GetFunctionName(const char* function)
{
    // TODO:: compute function name c_str at compile time
    return function;
}

/**
 * @brief record enter/leave a function
 */
class MINILOGGER_API LoggerGuard {
public:
    LoggerGuard(LoggerLevel level, const char* function, uint32_t line);
    ~LoggerGuard();
private:
    LoggerLevel     m_level;
    const char*     m_function;
    uint32_t        m_line;
};

/**
 * @brief provide a c++ stream style log
 */
class MINILOGGER_API LoggerStream : public std::ostringstream {
public:
    LoggerStream(LoggerLevel level, const char* function, uint32_t line);
    ~LoggerStream();
public:
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
    uint64_t timestamp = chrono::duration_cast<chrono::microseconds>(clock::now().time_since_epoch()).count(); 
    char messageBuffer[LOGGER_MESSAGE_BUFFER_MAX_LEN] = { '\0' };
    if (sizeof...(args) == 0) { // empty args optimization
        std::strncpy(messageBuffer, format, sizeof(messageBuffer) - 1);
        Logger::GetInstance()->KeepLog(level, function, line, messageBuffer, timestamp);
        return;
    }
    if (::snprintf(messageBuffer, LOGGER_MESSAGE_BUFFER_MAX_LEN, format, args...) < 0) {
        // TODO
        std::fill(messageBuffer, messageBuffer + LOGGER_MESSAGE_BUFFER_MAX_LEN, 0);
        std::strncpy(messageBuffer, "...", sizeof(messageBuffer) - 1);
    }
    Logger::GetInstance()->KeepLog(level, function, line, messageBuffer, timestamp);
}

}
}

#endif
