#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

// Static member definitions
std::unique_ptr<Logger> Logger::instance = nullptr;
std::mutex Logger::instanceMutex;

Logger::Logger() : currentLogLevel(LogLevel::Info), consoleOutput(true), fileOutput(true) {
}

Logger& Logger::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::unique_ptr<Logger>(new Logger());
    }
    return *instance;
}

void Logger::initialize(const std::string& logFilePath, LogLevel level, bool console, bool file) {
    auto& logger = getInstance();
    logger.currentLogLevel = level;
    logger.consoleOutput = console;
    logger.fileOutput = file;
    
    if (file && !logFilePath.empty()) {
        logger.logFile.open(logFilePath, std::ios::app);
        if (!logger.logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFilePath << std::endl;
            logger.fileOutput = false;
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
