#include <assert.h>
#include <cstdarg>
#include <cstdio>
#include <ios>
#include <system_error>
#include <thread>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include <ctime>

#include <zip.h>


// include windows filesystem releated headers
#ifdef _WIN32

#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// headers for convert UTF-8 UTF-16
#include <locale>
#include <codecvt>

#include <Windows.h>

#else

// include posix filesystem releated headers
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "Logger.h"

#ifdef _WIN32
    #define NEW_LINE "\r\n"; // CRLF
#else
    #define NEW_LINE "\n"; // LF
#endif

#ifdef _WIN32
    constexpr auto SEPARATOR = "\\";
#else
    constexpr auto SEPARATOR = "/";
#endif

using namespace xuranus::minilogger;

namespace {
    const uint32_t LOGGER_LEVEL_COUNT = 5;
    const uint32_t LOGGER_BUFFER_DEFAULT_LEN = LOGGER_MESSAGE_BUFFER_MAX_LEN + LOGGER_FUNCTION_BUFFER_MAX_LEN + 1024;
    
    // [datetime][level][message][function:line][threadID][threadLocalKey]
    const char* LOG_FORMAT_STR = "[%s.%u][%s][%s][%s:%u][%llu][%s]" NEW_LINE;
    const std::string MINILOGGER_ARCHIVE_FILE_EXTENSION = ".zip";
}

// TODO:: use the constexpr version
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

template<class... Args>
void InternalErrorLog(const char* format, Args... args)
{
    char messageBuffer[LOGGER_MESSAGE_BUFFER_MAX_LEN] = { '\0' };
    if (sizeof...(args) == 0) { // empty args optimization
        std::strncpy(messageBuffer, format, sizeof(messageBuffer) - 1);
    } else if (::snprintf(messageBuffer, LOGGER_MESSAGE_BUFFER_MAX_LEN, format, args...) < 0) {
        // FATAL internal error, should be checked at dev time
        return;
    }
    // TODO:: record message buffer into another place
}

#ifdef _WIN32
std::wstring Utf8ToUtf16(const std::string& str)
{
    using ConvertTypeX = std::codecvt_utf8_utf16<wchar_t>;
    std::wstring_convert<ConvertTypeX> converterX;
    std::wstring wstr = converterX.from_bytes(str);
    return wstr;
}

std::string Utf16ToUtf8(const std::wstring& wstr)
{
    using ConvertTypeX = std::codecvt_utf8_utf16<wchar_t>;
    std::wstring_convert<ConvertTypeX> converterX;
    return converterX.to_bytes(wstr);
}
#endif


/**
 * @brief mutiple-platform filesystem api
 */
namespace fsutility {

bool IsDirectory(const std::string& path)
{
#if defined (_WIN32)
    std::wstring wPath = Utf8ToUtf16(path);
    DWORD attribute = ::GetFileAttributesW(wPath.c_str());
    return attribute != INVALID_FILE_ATTRIBUTES && (attribute & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return (::stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0);
#endif
}

bool RenameFile(const std::string& oldPath, const std::string& newPath)
{
#if defined (_WIN32)
    return ::MoveFileW(Utf8ToUtf16(oldPath).c_str(), Utf8ToUtf16(newPath).c_str());
#else
    return ::rename(oldPath.c_str(), newPath.c_str()) == 0;
#endif
}

bool RemoveFile(const std::string& path)
{
#if defined (_WIN32)
    return ::DeleteFileW(Utf8ToUtf16(path).c_str());
#else
    return ::remove(path.c_str()) == 0;
#endif
}
}

/**
 * @brief Logger implementation, used to prevent header corruption
 */
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

    void SetThreadLocalKey(const std::string& key) override;

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
    std::string GetCurrentLogFilePath() const;
    std::string GenerateTempLogFilePath() const;
    std::string GenerateArchiveFilePath() const;
    void SwitchToNewLogFile();
    void AsyncCreateArchiveFile(const std::string& tempLogFilePath, const std::string& archiveFilePath);

private:
    LoggerLevel             m_level { LoggerLevel::DEBUG };
    CongestionControlPolicy m_congestionPolicy { CongestionControlPolicy::BLOCKING };
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
    "DBG",
    "INFO",
    "WARN",
    "ERR",
    "FATAL"
};

thread_local std::string g_threadLocalKey;

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

void LoggerImpl::SetThreadLocalKey(const std::string& key)
{
    g_threadLocalKey = key;
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
    if ((!m_inited && m_config.target != LoggerTarget::STDOUT) || m_abort) {
        return;
    }
    char bufferLocal[LOGGER_BUFFER_DEFAULT_LEN] = { '\0' };
    char* bufferEx = nullptr;
    char* buffer = bufferLocal;
    static std::hash<std::thread::id> ThreadIDHasher;
    uint64_t threadID = ThreadIDHasher(std::this_thread::get_id());
    std::string datetime = ParseDateTimeFromSeconds(timestamp / 1000000, m_timezoneTimestampOffset);
    uint32_t microSeconds = static_cast<uint32_t>(timestamp % 1000000);
    const char* levelStr = g_loggerLevelStr[static_cast<uint32_t>(level)];
    std::string prettyFunction = FormatFunction(function);
    int length = ::snprintf(buffer, LOGGER_BUFFER_DEFAULT_LEN, LOG_FORMAT_STR,
            datetime.c_str(), microSeconds, levelStr, message, prettyFunction.c_str(), line, threadID, g_threadLocalKey.c_str());
    if (length >= LOGGER_BUFFER_DEFAULT_LEN) {
        // truncated buffer other wise
        bufferEx = new char[length + 1];
        memset(bufferEx, 0, sizeof(char) * (length + 1));
        buffer = bufferEx;
        ::snprintf(bufferEx, length + 1, LOG_FORMAT_STR,
            datetime.c_str(), microSeconds, levelStr, message, prettyFunction.c_str(), line, threadID, g_threadLocalKey.c_str());
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

std::string LoggerImpl::GetCurrentLogFilePath() const
{
    return m_config.logDirPath + SEPARATOR + m_config.fileName;
}

/**
 * @brief generate a temp log file path to rename current log file
 */
std::string LoggerImpl::GenerateTempLogFilePath() const
{
    // temp file suffix need to be unique to handle concurrent rename case
    std::string timestamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::string tempFileSuffix = std::string(".") + timestamp + ".tmp";
    return m_config.logDirPath + SEPARATOR + m_config.fileName + tempFileSuffix;
}

/**
 * @brief generate a archive file path for current log file
 */
std::string LoggerImpl::GenerateArchiveFilePath() const
{
    std::string timestamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::string archiveFileName = m_config.archiveFileName + "." + timestamp + MINILOGGER_ARCHIVE_FILE_EXTENSION;
    std::string archiveFilePath = m_config.logDirPath + SEPARATOR + archiveFileName;
    return archiveFilePath;
}

bool LoggerImpl::InitLoggerFileOutput()
{
    try {
        if (!fsutility::IsDirectory(m_config.logDirPath)) {
            return false;
        }
        m_file.open(GetCurrentLogFilePath(), std::ios::binary | std::ios::app);
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
        if (m_file.tellp() >= m_config.fileSizeMax) {
            SwitchToNewLogFile();
        }
    }
    if (m_file) {
        m_file.flush();
        m_file.close();
    }
}

void LoggerImpl::SwitchToNewLogFile()
{
    if (m_file.is_open()) {
        m_file.close();
    }
    std::error_code ec;
    std::string currentLogFilePath = GetCurrentLogFilePath();
    std::string tempLogFilePath = GenerateTempLogFilePath();
    if (!fsutility::RenameFile(currentLogFilePath, tempLogFilePath)) {
        InternalErrorLog("failed to rename %s to %s",
            currentLogFilePath.c_str(), tempLogFilePath.c_str());
        return;
    }
    std::string archiveFilePath = GenerateArchiveFilePath();
    InitLoggerFileOutput();
    AsyncCreateArchiveFile(tempLogFilePath, archiveFilePath);
}

void LoggerImpl::AsyncCreateArchiveFile(const std::string& tempLogFilePath, const std::string& archiveFilePath)
{
    ::zip_t* archive = nullptr;
    archive = ::zip_open(archiveFilePath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, NULL);
    if (archive == nullptr) {
        InternalErrorLog("failed to open archive %s", archiveFilePath.c_str());
        return;
    }
    ::zip_source_t* source = ::zip_source_file(archive, tempLogFilePath.c_str(), 0, 0);
    if (source == nullptr) {
        InternalErrorLog("failed to source %s to archive file %s", archiveFilePath.c_str());
        ::zip_close(archive);
        return;
    }
    ::zip_file_add(archive, m_config.fileName.c_str(), source, ZIP_FL_ENC_UTF_8);
    ::zip_close(archive);
    if (!fsutility::RemoveFile(tempLogFilePath)) {
        InternalErrorLog("failed to remove temp file %s", tempLogFilePath.c_str());
        return;
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

LoggerStream::LoggerStream(LoggerLevel level, const char* function, uint32_t line)
 : m_level(level), m_function(function), m_line(line)
{}

LoggerStream::~LoggerStream()
{
    // check current log level
    if (!Logger::GetInstance()->ShouldKeepLog(m_level)) {
        return;
    }
    namespace chrono = std::chrono;
    using clock = std::chrono::system_clock;
    uint64_t timestamp = chrono::duration_cast<chrono::microseconds>(clock::now().time_since_epoch()).count(); 
    Logger::GetInstance()->KeepLog(m_level, m_function, m_line, this->str().c_str(), timestamp);
}
