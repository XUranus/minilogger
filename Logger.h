#ifndef LOGGER_HEADER_H
#define LOGGER_HEADER_H

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <mutex>

namespace xuranus {
namespace minilogger {

enum class LoggerLevel {
    DEBUG       = 0,
    INFO        = 1,
    WARNING     = 2,
    ERROR       = 3
};

#define DBGLOG(format, args...)     ::xuranus::minilogger::Logger::GetInstance().WriteLog(LoggerLevel::DEBUG, __PRETTY_FUNCTION__, __LINE__ ,format, ##args)
#define INFOLOG(format, args...)    ::xuranus::minilogger::Logger::GetInstance().WriteLog(LoggerLevel::INFO, __PRETTY_FUNCTION__, __LINE__,format, ##args)
#define WARNLOG(format, args...)    ::xuranus::minilogger::Logger::GetInstance().WriteLog(LoggerLevel::WARNING, __PRETTY_FUNCTION__, __LINE__, format, ##args)
#define ERRLOG(format, args...)     ::xuranus::minilogger::Logger::GetInstance().WriteLog(LoggerLevel::ERROR, __PRETTY_FUNCTION__, __LINE__, format, ##args)

class Logger {
public:
    static Logger& GetInstance();

    template<class... Args>
    void WriteLog(LoggerLevel level, const char* function, int line, const char* format, Args...);
    ~Logger();
private:
    Logger();

public:
    static Logger instance;
private:
    std::mutex m_mutex;
};


static std::string TimestampSecondsToDate(uint64_t timestamp)
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

template<class... Args>
void Logger::WriteLog(LoggerLevel level, const char* function, int line, const char* format, Args... args)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string date = TimestampSecondsToDate(std::chrono::system_clock::now().time_since_epoch().count() / std::chrono::system_clock::period::den);
    ::printf("[%s][", date.c_str());
    ::printf(format, args...);
    ::printf("][%s:%d]\n", function, line);
}

}
}

#endif