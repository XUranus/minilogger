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
#include "Logger.h"

using namespace xuranus::minilogger;
namespace fs = std::filesystem;

namespace {
    const uint32_t LOGGER_LEVEL_COUNT = 5;
    const uint32_t LOGGER_BUFFER_MAX_LEN = LOGGER_MESSAGE_BUFFER_MAX_LEN + LOGGER_FUNCTION_BUFFER_MAX_LEN + 1024;
}

// utils function
static std::string ParseDateTime(uint64_t sec)
{
    namespace chrono = std::chrono;
    std::ostringstream oss;
    auto seconds = chrono::duration_cast<chrono::seconds>(chrono::seconds(sec));
    auto timePoint = std::chrono::system_clock::time_point{} + seconds;
    std::time_t timestamp = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* timeinfo = std::localtime(&timestamp);
    if (timeinfo != nullptr) {
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
    bool                    m_inited { false };
    LoggerConfig            m_config;
    std::ofstream           m_file;

    std::mutex              m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
    char*                   m_frontendBuffer { nullptr };
    char*                   m_backendBuffer { nullptr };

    std::atomic<uint64_t>   m_frontendBufferOffset { 0 };
    std::atomic<uint64_t>   m_backendBufferOffset { 0 };

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

bool LoggerImpl::ShouldKeepLog(LoggerLevel level) const
{
    return m_level <= level;
}

void LoggerImpl::Destroy()
{
    m_abort = true;
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
    char buffer[LOGGER_BUFFER_MAX_LEN] = { '\0' };
    std::thread::id threadID = std::this_thread::get_id();
    std::string datetime = ParseDateTime(timestamp);
    const char* levelStr = g_loggerLevelStr[static_cast<uint32_t>(level)];
    std::string prettyFunction = FormatFunction(function);
    // [datetime][level][message][function:line][threadID]
    const char* format = "[%s][%s][%s][%s:%u][%lu]\n";
    int length = ::snprintf(buffer, LOGGER_BUFFER_MAX_LEN, format,
            datetime.c_str(), levelStr, message, prettyFunction.c_str(), line, threadID);
    if (length >= LOGGER_BUFFER_MAX_LEN) {
        // TODO:: truncated buffer
    }
    if (m_config.target == LoggerTarget::STDOUT) {
        printf("%s", buffer);
    } else {
        std::unique_lock<std::mutex> lk(m_mutex);
        // lock thread util frontendBufferOffset + length < bufferSize
        m_notFull.wait(lk, [&]() {
            return m_frontendBufferOffset + length < m_config.bufferSize; 
        });
        // write n bytes to frontendBuffer from offset
        memcpy(m_frontendBuffer + m_frontendBufferOffset, buffer, length);
        m_frontendBufferOffset += length;
        m_notEmpty.notify_one();
    }
}

bool LoggerImpl::Init(const LoggerConfig& conf)
{
    if (m_inited) {
        return true;
    }
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
        m_file.open(filepath.string(), std::ios::out | std::ios::binary | std::ios::app);
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
        std::unique_lock<std::mutex> lk(m_mutex);
       
        m_notEmpty.wait(lk, [&]() {
            return m_abort || m_frontendBufferOffset != 0;
        });
        if (m_frontendBufferOffset == 0) {
            // unblocked due to abort
            break;
        }
        // switch buffer
        std::swap(m_frontendBuffer, m_backendBuffer);
        m_backendBufferOffset = m_frontendBufferOffset.load();
        m_frontendBufferOffset = 0;
        // frontend threads can be recovered
        m_notFull.notify_all();
        lk.unlock();
        // start I/O
        m_file.write(m_backendBuffer, m_backendBufferOffset.load());
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