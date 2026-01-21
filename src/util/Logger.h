/**
 * Clarity Layer - Logger
 * Centralized logging with file and debug output
 */

#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace clarity {

class Logger {
public:
    // Initialize logging system
    static void Initialize();

    // Shutdown logging system
    static void Shutdown();

    // Get the logger instance
    static std::shared_ptr<spdlog::logger>& Get();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace clarity

// Convenience macros for logging
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(clarity::Logger::Get(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(clarity::Logger::Get(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(clarity::Logger::Get(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(clarity::Logger::Get(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(clarity::Logger::Get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(clarity::Logger::Get(), __VA_ARGS__)
