#pragma once
/**
 * NetForge — Logger.hpp
 *
 * Thread-safe, header-only logger using std::mutex + std::ostream.
 * Levels: DEBUG, INFO, WARN, ERROR.  Timestamps every line.
 */

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>

namespace netforge {

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERR = 3 };

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // Set the minimum level that actually prints.
    void setLevel(LogLevel level) noexcept { minLevel_ = level; }

    template <typename... Args>
    void log(LogLevel level, Args&&... args) {
        if (level < minLevel_) return;

        std::ostringstream oss;
        oss << '[' << timestamp() << "] [" << levelTag(level) << "] ";
        (oss << ... << std::forward<Args>(args));
        oss << '\n';

        std::lock_guard lock(mutex_);
        std::clog << oss.str();
    }

    // Convenience helpers
    template <typename... Args> void debug(Args&&... args) { log(LogLevel::DEBUG, std::forward<Args>(args)...); }
    template <typename... Args> void info (Args&&... args) { log(LogLevel::INFO,  std::forward<Args>(args)...); }
    template <typename... Args> void warn (Args&&... args) { log(LogLevel::WARN,  std::forward<Args>(args)...); }
    template <typename... Args> void error(Args&&... args) { log(LogLevel::ERR, std::forward<Args>(args)...); }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::string timestamp() {
        using namespace std::chrono;
        auto now   = system_clock::now();
        auto timer = system_clock::to_time_t(now);
        auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::ostringstream ss;
        std::tm bt{};
#ifdef _WIN32
        localtime_s(&bt, &timer);
#else
        localtime_r(&timer, &bt);
#endif
        ss << std::put_time(&bt, "%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    static constexpr std::string_view levelTag(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERR:   return "ERROR";
        }
        return "?????";
    }

    std::mutex mutex_;
    LogLevel   minLevel_{LogLevel::INFO};
};

// Global convenience macros (file/line context stripped for brevity in prod)
#define NF_DEBUG(...) ::netforge::Logger::instance().debug(__VA_ARGS__)
#define NF_INFO(...)  ::netforge::Logger::instance().info (__VA_ARGS__)
#define NF_WARN(...)  ::netforge::Logger::instance().warn (__VA_ARGS__)
#define NF_ERROR(...) ::netforge::Logger::instance().error(__VA_ARGS__)

} // namespace netforge
