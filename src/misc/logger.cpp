#include "logger.h"

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef _WIN32
#include <windows.h>
// LogLevel uses ERR (not ERROR) to avoid conflict with Windows ERROR macro
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace ldt {

namespace {

std::mutex g_logSinkMutex;
size_t g_nextSinkId = 1;
std::unordered_map<size_t, ILogSink*> g_logSinks;
thread_local bool g_isDispatchingToSinks = false;

class LoggerService final : public ILogger {
public:
    void write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os) override {
        Logger::write(category, level, color, msg, os);
    }

    LogSinkSubscription subscribe(ILogSink& sink) override {
        return Logger::subscribe(sink);
    }
};

LoggerService g_loggerService;

void notifySinks(const LogMessage& message) {
    if (g_isDispatchingToSinks) {
        return;
    }

    std::vector<ILogSink*> sinks;
    {
        std::lock_guard<std::mutex> lock(g_logSinkMutex);
        sinks.reserve(g_logSinks.size());
        for (const auto& entry : g_logSinks) {
            sinks.push_back(entry.second);
        }
    }

    g_isDispatchingToSinks = true;
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onLogMessage(message);
        }
    }
    g_isDispatchingToSinks = false;
}

} // namespace

LogSinkSubscription::LogSinkSubscription(size_t sinkId)
    : sinkId_(sinkId) {}

LogSinkSubscription::~LogSinkSubscription() {
    reset();
}

LogSinkSubscription::LogSinkSubscription(LogSinkSubscription&& other) noexcept
    : sinkId_(other.sinkId_) {
    other.sinkId_ = 0;
}

LogSinkSubscription& LogSinkSubscription::operator=(LogSinkSubscription&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    reset();
    sinkId_ = other.sinkId_;
    other.sinkId_ = 0;
    return *this;
}

void LogSinkSubscription::reset() {
    if (sinkId_ == 0) {
        return;
    }

    Logger::unsubscribe(sinkId_);
    sinkId_ = 0;
}

ILogger& getLogger() {
    return g_loggerService;
}

void Logger::setColor(LogColor color, std::ostream& os) {
#ifdef _WIN32
    static HANDLE hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    static HANDLE hConsoleErr = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE hConsole = (&os == &std::cerr) ? hConsoleErr : hConsoleOut;

    int winColor = 0;
    switch (color) {
    case LogColor::RED:
        winColor = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case LogColor::GREEN:
        winColor = FOREGROUND_GREEN;
        break;
    case LogColor::YELLOW:
        winColor = FOREGROUND_RED | FOREGROUND_GREEN;
        break;
    case LogColor::DEFAULT:
    default:
        winColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    }
    SetConsoleTextAttribute(hConsole, winColor);
#else
    switch (color) {
    case LogColor::RED: os << "\033[1;31m"; break;
    case LogColor::GREEN: os << "\033[1;32m"; break;
    case LogColor::YELLOW: os << "\033[1;33m"; break;
    case LogColor::DEFAULT:
    default: os << "\033[0m"; break;
    }
#endif
}

void Logger::resetColor(std::ostream& os) {
#ifdef _WIN32
    setColor(LogColor::DEFAULT, os);
#elif defined(__ANDROID__)
    (void)os;
#else
    os << "\033[0m";
#endif
}

void Logger::write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os) {
    writeLine(category, level, color, msg, os);
}

void Logger::writeLine(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os) {
    if (!shouldLog(category, level)) {
        return;
    }

    const bool isError = (&os == &std::cerr);

    // 构建带等级和分类前缀的消息: [LEVEL][CAT] message
    std::string tagged;
    tagged.reserve(20 + msg.size());
    tagged += "[";
    tagged += logLevelToString(level);
    tagged += "][";
    tagged += logCategoryToString(category);
    tagged += "] ";
    tagged += msg;

#ifdef __ANDROID__
    (void)os;
    int priority = ANDROID_LOG_INFO;
    if (color == LogColor::RED || isError) {
        priority = ANDROID_LOG_ERROR;
    } else if (color == LogColor::YELLOW) {
        priority = ANDROID_LOG_WARN;
    }
    __android_log_write(priority, "LDT", tagged.c_str());
#else
    setColor(color, os);
    os << tagged;
    resetColor(os);
    os << std::endl;
#endif

    notifySinks(LogMessage{category, level, color, tagged, isError});
}

} // namespace ldt

namespace ldt {

bool& Logger::enabled() {
    static bool e = true;
    return e;
}

void Logger::setEnabled(bool e) {
    enabled() = e;
}

LogLevel& Logger::logLevel() {
    static LogLevel level = LogLevel::TRACE;
    return level;
}

void Logger::setLogLevel(LogLevel level) {
    logLevel() = level;
}

bool Logger::shouldLog(LogLevel level) {
    if (!enabled()) {
        return false;
    }
    return static_cast<int>(level) >= static_cast<int>(logLevel());
}

LogCategory& Logger::logCategory() {
    static LogCategory cat = LogCategory::USER;
    return cat;
}

void Logger::setLogCategory(LogCategory category) {
    logCategory() = category;
}

bool Logger::shouldLog(LogCategory category, LogLevel level) {
    if (!enabled()) {
        return false;
    }
    if ((static_cast<int>(category) & static_cast<int>(logCategory())) == 0) {
        return false;
    }
    return static_cast<int>(level) >= static_cast<int>(logLevel());
}

LogSinkSubscription Logger::subscribe(ILogSink& sink) {
    std::lock_guard<std::mutex> lock(g_logSinkMutex);
    const size_t sinkId = g_nextSinkId++;
    g_logSinks.emplace(sinkId, &sink);
    return LogSinkSubscription(sinkId);
}

void Logger::unsubscribe(size_t sinkId) {
    if (sinkId == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_logSinkMutex);
    g_logSinks.erase(sinkId);
}

const char* logLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::TRACE: return "TRACE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO";
    case LogLevel::WARN:  return "WARN";
    case LogLevel::ERR:   return "ERROR";
    case LogLevel::FATAL: return "FATAL";
    default:              return "UNKNOWN";
    }
}

const char* logCategoryToString(LogCategory category) {
    switch (category) {
    case LogCategory::USER:   return "USER";
    case LogCategory::SYSTEM: return "SYS";
    case LogCategory::ALL:    return "ALL";
    default:                  return "?";
    }
}

} // namespace ldt
