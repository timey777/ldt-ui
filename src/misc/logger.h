#pragma once
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "ldt_export.h"

// Windows-specific terminal color handling is implemented in logger.cpp

// Windows compatibility: windows.h #defines ERROR, so we use ERR instead.
// Permanently undefine it here — use GetLastError() for Windows error codes.
#ifdef ERROR
#undef ERROR
#endif

namespace ldt {

    // 日志分类（位掩码，可组合过滤）
    enum class LogCategory : int {
        USER   = 1 << 0,  // 用户/业务日志（默认）
        SYSTEM = 1 << 1,  // 引擎/诊断日志
        ALL    = USER | SYSTEM
    };

    // 日志级别枚举（数值越大越严重）
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO  = 2,
        WARN  = 3,
        ERR   = 4,
        FATAL = 5
    };

    // 颜色枚举
    enum class LogColor {
        DEFAULT,
        RED,
        GREEN,
        YELLOW
    };

    struct LDT_API LogMessage {
        LogCategory category = LogCategory::USER;
        LogLevel level = LogLevel::INFO;
        LogColor color = LogColor::DEFAULT;
        std::string text;
        bool isError = false;
    };

    class LDT_API ILogSink {
    public:
        virtual ~ILogSink() = default;
        virtual void onLogMessage(const LogMessage& message) = 0;
    };

    class LDT_API LogSinkSubscription {
    public:
        LogSinkSubscription() = default;
        ~LogSinkSubscription();

        LogSinkSubscription(const LogSinkSubscription&) = delete;
        LogSinkSubscription& operator=(const LogSinkSubscription&) = delete;

        LogSinkSubscription(LogSinkSubscription&& other) noexcept;
        LogSinkSubscription& operator=(LogSinkSubscription&& other) noexcept;

        bool valid() const { return sinkId_ != 0; }
        void reset();

    private:
        explicit LogSinkSubscription(size_t sinkId);

        size_t sinkId_ = 0;

        friend class Logger;
    };

    class LDT_API ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os = std::cout) = 0;
        virtual LogSinkSubscription subscribe(ILogSink& sink) = 0;
    };

    LDT_API ILogger& getLogger();

    class LDT_API Logger {
    private:
        // 设置颜色
        static void setColor(LogColor color, std::ostream& os);

        // reset color
        static void resetColor(std::ostream& os);

        static void writeLine(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os);

    public:
        // 启用/禁用日志（master switch）
        static bool& enabled();

        static void setEnabled(bool e);

        // 日志级别过滤
        static LogLevel& logLevel();

        static void setLogLevel(LogLevel level);

        static bool shouldLog(LogLevel level);

        // 日志分类过滤（默认 ALL，显示所有分类）
        static LogCategory& logCategory();

        static void setLogCategory(LogCategory category);

        static bool shouldLog(LogCategory category, LogLevel level);

        // 写入日志（带分类和级别）
        static void write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os = std::cout);

        static LogSinkSubscription subscribe(ILogSink& sink);

        static void unsubscribe(size_t sinkId);

        // 带颜色的输出
        template<typename T>
        static void logWithColor(LogColor color, const T& msg, std::ostream& os = std::cout) {
            if (&os == &std::cout && !enabled()) return;

            std::ostringstream oss;
            oss << msg;
            write(LogCategory::USER, LogLevel::INFO, color, oss.str(), os);
        }
    };

    // 便捷函数
    LDT_API const char* logLevelToString(LogLevel level);
    LDT_API const char* logCategoryToString(LogCategory category);

} // namespace ldt

// 兼容且支持流表达式的宏实现（按日志级别过滤，默认 USER 分类）
#define LDT_LOG(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::INFO)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::INFO, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_LOG_RAW(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::INFO)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::INFO, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_TRACE(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::TRACE)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::TRACE, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_DEBUG(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::DEBUG)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::DEBUG, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_INFO(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::INFO)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::INFO, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_WARN(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::WARN)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::WARN, ldt::LogColor::YELLOW, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_ERROR(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::ERR)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::ERR, ldt::LogColor::RED, _ldt_oss.str(), std::cerr); \
        } \
    } while(0)

#define LDT_ERROR_RED(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::ERR)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::ERR, ldt::LogColor::RED, _ldt_oss.str(), std::cerr); \
        } \
    } while(0)

#define LDT_SUCCESS(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::INFO)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::INFO, ldt::LogColor::GREEN, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_FATAL(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::USER, ldt::LogLevel::FATAL)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::USER, ldt::LogLevel::FATAL, ldt::LogColor::RED, _ldt_oss.str(), std::cerr); \
        } \
    } while(0)

// === SYSTEM 分类宏（引擎诊断用） ===
#define LDT_TRACE_SYS(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::SYSTEM, ldt::LogLevel::TRACE)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::SYSTEM, ldt::LogLevel::TRACE, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_DEBUG_SYS(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::SYSTEM, ldt::LogLevel::DEBUG)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::SYSTEM, ldt::LogLevel::DEBUG, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)

#define LDT_INFO_SYS(x) \
    do { \
        if (ldt::Logger::shouldLog(ldt::LogCategory::SYSTEM, ldt::LogLevel::INFO)) { \
            std::ostringstream _ldt_oss; \
            _ldt_oss << x; \
            ldt::getLogger().write(ldt::LogCategory::SYSTEM, ldt::LogLevel::INFO, ldt::LogColor::DEFAULT, _ldt_oss.str(), std::cout); \
        } \
    } while(0)
