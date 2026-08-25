#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

// Static member definitions
std::unique_ptr<Logger> Logger::instance = nullptr;
std::mutex Logger::instanceMutex;

Logger::Logger() : currentLogLevel(LogLevel::Info), consoleOutput(true), fileOutput(true),
                   maxFileSizeBytes(5 * 1024 * 1024), maxBackupFiles(3) {
}

Logger& Logger::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::unique_ptr<Logger>(new Logger());
    }
    return *instance;
}

void Logger::initialize(const std::string& path, LogLevel level, bool console, bool file,
                        size_t fileSizeLimit, int backupCount) {
    auto& logger = getInstance();
    logger.currentLogLevel = level;
    logger.consoleOutput = console;
    logger.fileOutput = file;
    logger.logFilePath = path;
    logger.maxFileSizeBytes = fileSizeLimit;
    logger.maxBackupFiles = backupCount;

    if (file && !path.empty()) {
        if (logger.logFile.is_open()) {
            logger.logFile.close();
        }
        logger.logFile.open(path, std::ios::app);
        if (!logger.logFile.is_open()) {
            std::cerr << "Failed to open log file: " << path << std::endl;
            logger.fileOutput = false;
        } else {
            logger.fileOutput = true;
        }
    }
}

// Rolls the log over once it exceeds maxFileSizeBytes: current -> ".1",
// ".1" -> ".2", ... keeping at most maxBackupFiles backups. Any failure is
// non-fatal; logging simply continues into the existing file.
void Logger::RotateIfNeeded() {
    if (!logFile.is_open() || maxFileSizeBytes == 0 || logFilePath.empty()) {
        return;
    }
    try {
        namespace fs = std::filesystem;
        if (!fs::exists(logFilePath)) {
            return;
        }
        auto size = fs::file_size(logFilePath);
        if (size < maxFileSizeBytes) {
            return;
        }

        logFile.close();
        const fs::path basePath(logFilePath);

        for (int i = maxBackupFiles - 1; i >= 1; --i) {
            fs::path from = basePath.string() + "." + std::to_string(i);
            fs::path to = basePath.string() + "." + std::to_string(i + 1);
            std::error_code ec;
            if (fs::exists(from)) {
                fs::remove(to, ec);
                fs::rename(from, to, ec);
            }
        }

        std::error_code ec;
        fs::rename(basePath, basePath.string() + ".1", ec);
        // On failure keep appending to the same file rather than losing logs.

        logFile.open(logFilePath, std::ios::app);
    } catch (...) {
        // Never let rotation problems break logging.
        if (!logFile.is_open() && !logFilePath.empty()) {
            logFile.open(logFilePath, std::ios::app);
        }
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0) {
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    }
    
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case Debug:   return "DEBUG";
        case Info:    return "INFO";
        case Warning: return "WARNING";
        case Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLogLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    RotateIfNeeded();
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);
    std::string logEntry = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    if (consoleOutput) {
        if (level >= LogLevel::Error) {
            std::cerr << logEntry << std::endl;
        } else {
            std::cout << logEntry << std::endl;
        }
    }
    
    if (fileOutput && logFile.is_open()) {
        logFile << logEntry << std::endl;
        logFile.flush();
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void Logger::setLogLevel(LogLevel level) {
    currentLogLevel = level;
}

void Logger::enableConsoleOutput(bool enable) {
    consoleOutput = enable;
}

void Logger::enableFileOutput(bool enable) {
    fileOutput = enable;
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}
