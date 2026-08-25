#pragma once
#ifndef Logger_H
#define Logger_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

enum LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
private:
    static std::unique_ptr<Logger> instance;
    static std::mutex instanceMutex;

    std::ofstream logFile;
    std::mutex logMutex;
    LogLevel currentLogLevel;
    bool consoleOutput;
    bool fileOutput;
    std::string logFilePath;
    size_t maxFileSizeBytes;
    int maxBackupFiles;

    Logger();
    std::string getCurrentTimestamp();
    std::string levelToString(LogLevel level);
    void RotateIfNeeded();

public:
    static Logger& getInstance();
    static void initialize(const std::string& logFilePath, LogLevel level = Info,
                         bool console = true, bool file = true,
                         size_t maxFileSizeBytes = 5 * 1024 * 1024,
                         int maxBackupFiles = 3);

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    void setLogLevel(LogLevel level);
    void enableConsoleOutput(bool enable);
    void enableFileOutput(bool enable);

    ~Logger();
};

// Convenience macros for logging
#define LOG_DEBUG(message) Logger::getInstance().debug(message)
#define LOG_INFO(message) Logger::getInstance().info(message)
#define LOG_WARNING(message) Logger::getInstance().warning(message)
#define LOG_ERROR(message) Logger::getInstance().error(message)

#endif
