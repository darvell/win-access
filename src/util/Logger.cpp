/**
 * Clarity Layer - Logger Implementation
 */

#include "Logger.h"

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace clarity {

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::Initialize() {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // Get log file path in AppData\Local\ClarityLayer\logs
        wchar_t* appDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataPath))) {
            std::filesystem::path logDir = std::filesystem::path(appDataPath) / L"ClarityLayer" / L"logs";
            CoTaskMemFree(appDataPath);

            // Create directory if it doesn't exist
            std::filesystem::create_directories(logDir);

            // Create log file with timestamp
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);

            wchar_t filename[64];
            wcsftime(filename, sizeof(filename) / sizeof(wchar_t), L"clarity_%Y%m%d_%H%M%S.log", &tm);

            auto logPath = logDir / filename;
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
            fileSink->set_level(spdlog::level::trace);
            sinks.push_back(fileSink);

            // Clean up old log files (keep last 10)
            std::vector<std::filesystem::path> logFiles;
            for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
                if (entry.path().extension() == L".log") {
                    logFiles.push_back(entry.path());
                }
            }
            if (logFiles.size() > 10) {
                std::sort(logFiles.begin(), logFiles.end());
                for (size_t i = 0; i < logFiles.size() - 10; ++i) {
                    std::filesystem::remove(logFiles[i]);
                }
            }
        }

        // MSVC debug output sink (for Visual Studio)
        #ifdef _DEBUG
        auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        msvcSink->set_level(spdlog::level::debug);
        sinks.push_back(msvcSink);
        #endif

        // Create logger with all sinks
        s_logger = std::make_shared<spdlog::logger>("clarity", sinks.begin(), sinks.end());
        s_logger->set_level(spdlog::level::trace);
        s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

        // Register as default logger
        spdlog::set_default_logger(s_logger);

        // Flush on warnings and above
        s_logger->flush_on(spdlog::level::warn);

        s_logger->info("Logger initialized");
    }
    catch (const spdlog::spdlog_ex& ex) {
        // Fall back to console logging
        s_logger = spdlog::stdout_color_mt("clarity");
        s_logger->error("Failed to initialize file logger: {}", ex.what());
    }
}

void Logger::Shutdown() {
    if (s_logger) {
        s_logger->info("Logger shutting down");
        s_logger->flush();
        s_logger.reset();
    }
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger>& Logger::Get() {
    if (!s_logger) {
        // Emergency fallback if logger not initialized
        s_logger = spdlog::stdout_color_mt("clarity_fallback");
    }
    return s_logger;
}

} // namespace clarity
