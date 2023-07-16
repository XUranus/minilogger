#include <cstdarg>
#include <cstdio>
#include <thread>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include <ctime>

#include "Logger.h"

using namespace xuranus::minilogger;
namespace fs = std::filesystem;

namespace {
    const uint32_t LOGGER_LEVEL_COUNT = 5;
    const uint32_t LOGGER_BUFFER_DEFAULT_LEN = LOGGER_MESSAGE_BUFFER_MAX_LEN + LOGGER_FUNCTION_BUFFER_MAX_LEN + 1024;
}

static std::string FormatFunction(const char* functionMacroLiteral)
{
    std::string function = functionMacroLiteral;
    if (function.length() >= LOGGER_FUNCTION_BUFFER_MAX_LEN) {
        function = function.substr(0, LOGGER_FUNCTION_BUFFER_MAX_LEN);
    }
    return function;
}

static int GetCurrentTimezoneOffset()
{
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);
    std::time_t utcTime = std::mktime(localTime);
    std::time_t localUtcTime = std::mktime(std::gmtime(&currentTime));
    int timezoneOffset = static_cast<int>(std::difftime(localUtcTime, utcTime) / 3600);
    return -timezoneOffset;
}

// Function to format a timestamp to a datetime string
static std::string ParseDateTimeFromSeconds(uint64_t timestamp, uint64_t timestampOffset)
{
    timestamp += timestampOffset;
    // Calculate the individual components
    int64_t seconds = timestamp % 60;
    timestamp /= 60;
    int64_t minutes = timestamp % 60;
    timestamp /= 60;
    int64_t hours = timestamp % 24;
    timestamp /= 24;
    int64_t days = timestamp;

    // Calculate the year
    int64_t year = 1970;  // Assuming the Unix epoch (January 1, 1970) as the starting point
    while (days >= 365) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            if (days >= 366) {
                days -= 366;
                year++;
            } else {
                break;
            }
        } else {
            days -= 365;
            year++;
        }
    }

    // Calculate the month and day
    int64_t monthDays[] = {
        31,
        ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int64_t month = 1;
    int64_t day = 1;
    for (; month <= 12; month++) {
        if (days < monthDays[month - 1]) {
            day += days;
            break;
        } else {
            days -= monthDays[month - 1];
        }
    }

    // Build the datetime string
    std::string datetime = std::to_string(year) + "-";
    if (month < 10) {
        datetime += "0";
    }
    datetime += std::to_string(month) + "-";
    if (day < 10) {
        datetime += "0";
    }
    datetime += std::to_string(day) + " ";

    if (hours < 10) {
        datetime += "0";
    }
    datetime += std::to_string(hours) + ":";
    if (minutes < 10) {
        datetime += "0";
    }
    datetime += std::to_string(minutes) + ":";
    if (seconds < 10) {
        datetime += "0";
    }
    datetime += std::to_string(seconds);

    return datetime;
}

// to prevent header corruption
class LoggerImpl : public Logger {
public:
    LoggerImpl();

    // implement Logger interface
    void KeepLog(
        LoggerLevel     level,
        const char*     function,
        uint32_t        line,
        const char*     message,
        uint64_t        timestamp) override;

    bool ShouldKeepLog(LoggerLevel level) const override;

    void SetLogLevel(LoggerLevel level) override;

    void SetCongestionControlPolicy(CongestionControlPolicy policy) override;

    bool Init(const LoggerConfig& conf) override;

    void Destroy() override;

    ~LoggerImpl();

private:
    void ResetBuffer();
    bool InitLoggerFileOutput();
    bool InitLoggerBuffer();
    bool StartConsumerThread();
    void ConsumerThread();

private:
    LoggerLevel             m_level { LoggerLevel::DEBUG };
    CongestionControlPolicy m_congestionPolicy { CongestionControlPolicy::DROPPING };
    bool                    m_inited { false };
    LoggerConfig            m_config;
    std::ofstream           m_file;
    uint64_t                m_timezoneTimestampOffset { 0 }; // timestamp timezone offset in seconds

    std::mutex              m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
    char*                   m_frontendBuffer { nullptr };
    char*                   m_backendBuffer { nullptr };

    uint64_t                m_frontendBufferOffset { 0 };
    uint64_t                m_backendBufferOffset { 0 };

    std::thread             m_consumerThread;
    bool                    m_abort { false };
};

// singleton instance using eager mode
static LoggerImpl instance;

const char* g_loggerLevelStr[LOGGER_LEVEL_COUNT] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

Logger* Logger::GetInstance()
{
    return dynamic_cast<Logger*>(&instance);
}

Logger::~Logger()
{}

// implement LoggerImpl from here
void LoggerImpl::SetLogLevel(LoggerLevel level)
{
    m_level = level;
}

void LoggerImpl::SetCongestionControlPolicy(CongestionControlPolicy policy)
{
    m_congestionPolicy = policy;
}

bool LoggerImpl::ShouldKeepLog(LoggerLevel level) const
{
    return m_level <= level;
}

void LoggerImpl::Destroy()
{
    // to stop consumer thread
    m_abort = true;
    m_notEmpty.notify_one();
    if (m_consumerThread.joinable()) {
        m_consumerThread.join();
    }
    ResetBuffer();
}

void LoggerImpl::ResetBuffer()
{
    if (m_frontendBuffer != nullptr) {
        delete[] m_frontendBuffer;
        m_frontendBuffer = nullptr;
    }
    if (m_backendBuffer == nullptr) {
        delete[] m_backendBuffer;
        m_backendBuffer = nullptr;
    }
}

LoggerImpl::LoggerImpl()
{}

LoggerImpl::~LoggerImpl()
{
    Destroy();
}

void LoggerImpl::KeepLog(
    LoggerLevel     level,
    const char*     function,
    uint32_t        line,
    const char*     message,
    uint64_t        timestamp)
{
    if (!m_inited || m_abort) {
        return;
    }
    char bufferLocal[LOGGER_BUFFER_DEFAULT_LEN] = { '\0' };
    char* bufferEx = nullptr;
    char* buffer = bufferLocal;
    std::thread::id threadID = std::this_thread::get_id();
    std::string datetime = ParseDateTimeFromSeconds(timestamp / 1000000, m_timezoneTimestampOffset);
    uint32_t microSeconds = static_cast<uint32_t>(timestamp % 1000000);
    const char* levelStr = g_loggerLevelStr[static_cast<uint32_t>(level)];
    std::string prettyFunction = FormatFunction(function);
    // [datetime][level][message][function:line][threadID]
    const char* format = "[%s.%u][%s][%s][%s:%u][%lu]\n";
    int length = ::snprintf(buffer, LOGGER_BUFFER_DEFAULT_LEN, format,
            datetime.c_str(), microSeconds, levelStr, message, prettyFunction.c_str(), line, threadID);
    if (length >= LOGGER_BUFFER_DEFAULT_LEN) {
        // truncated buffer other wise
        bufferEx = new char[length + 1];
        memset(bufferEx, 0, sizeof(char) * (length + 1));
        buffer = bufferEx;
        ::snprintf(bufferEx, length + 1, format,
            datetime.c_str(), levelStr, message, prettyFunction.c_str(), line, threadID);
    }
    if (m_config.target == LoggerTarget::STDOUT) {
        // do not buffering for stdout output
        printf("%s", buffer);
    } else {
        std::unique_lock<std::mutex> lk(m_mutex);
        // lock thread util frontendBufferOffset + length < bufferSize
        if (m_frontendBufferOffset + length >= m_config.bufferSize &&
            m_congestionPolicy == CongestionControlPolicy::DROPPING) {
            // dropping policy take effect here, current log will be dropped
            return;
        }
        m_notFull.wait(lk, [&]() {
            return m_frontendBufferOffset + length < m_config.bufferSize; 
        });
        // write n bytes to frontendBuffer from offset
        memcpy(m_frontendBuffer + m_frontendBufferOffset, buffer, length);
        m_frontendBufferOffset += length;
        m_notEmpty.notify_one();
    }
    if (bufferEx != nullptr) {
        delete[] bufferEx;
        bufferEx = nullptr;
    }
}

bool LoggerImpl::Init(const LoggerConfig& conf)
{
    if (m_inited) {
        return true;
    }
    m_timezoneTimestampOffset = GetCurrentTimezoneOffset() * 60 * 60;
    m_config = conf;
    if (m_config.target == LoggerTarget::FILE) {
        if (InitLoggerFileOutput() &&
            InitLoggerBuffer() &&
            StartConsumerThread()) {
            m_inited = true;
        } else {
            m_inited = false;
        }
    } else {
        m_inited = true;
    }
    return m_inited;
}

bool LoggerImpl::InitLoggerBuffer()
{
    if (m_config.bufferSize > LOGGER_BUFFER_SIZE_MAX / 2) {
        return false;
    }
    ResetBuffer();
    m_frontendBuffer = new (std::nothrow) char[m_config.bufferSize];
    if (m_frontendBuffer == nullptr) {
        return false;
    }
    m_backendBuffer = new (std::nothrow) char[m_config.bufferSize];
    if (m_backendBuffer == nullptr) {
        delete[] m_frontendBuffer;
        m_frontendBuffer = nullptr;
        return false;
    }
    return true;
}

bool LoggerImpl::InitLoggerFileOutput()
{
    try {
        if (!fs::is_directory(m_config.logDirPath)) {
            return false;
        }
        fs::path filepath = fs::path(m_config.logDirPath) / fs::path(m_config.fileName);
        m_file.open(filepath.string(), std::ios::binary | std::ios::app);
        if (!m_file) {
            return false;
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool LoggerImpl::StartConsumerThread()
{
    try {
        m_consumerThread = std::thread(&LoggerImpl::ConsumerThread, this);
    } catch (...) {
        return false;
    }
    return true;
}

void LoggerImpl::ConsumerThread()
{
    while (true) {
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_notEmpty.wait(lk, [&]() { return m_abort || m_frontendBufferOffset != 0; });
            if (m_frontendBufferOffset == 0) {
                // unblocked due to abort
                break;
            }
            // switch buffer
            std::swap(m_frontendBuffer, m_backendBuffer);
            m_backendBufferOffset = m_frontendBufferOffset;
            m_frontendBufferOffset = 0;
            // frontend threads can be recovered
            m_notFull.notify_all();
        }
        // start I/O
        m_file.write(m_backendBuffer, m_backendBufferOffset);
        if (m_abort) {
            break;
        }
    }
    if (m_file) {
        m_file.flush();
        m_file.close();
    }
}

// implement LoggerGuard from here
LoggerGuard::LoggerGuard(LoggerLevel level, const char* function, uint32_t line)
 : m_level(level), m_function(function), m_line(line)
{
    Log(level, function, line, "Logger Guard, Enter %s", function);
}

LoggerGuard::~LoggerGuard()
{
    Log(m_level, m_function, m_line, "Logger Guard, Exit %s", m_function);
}