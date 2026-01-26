#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

// Static member definitions
std::unique_ptr<ConfigManager> ConfigManager::instance = nullptr;
std::mutex ConfigManager::instanceMutex;

ConfigManager::ConfigManager() {
    setDefaults();
}

ConfigManager& ConfigManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance = std::unique_ptr<ConfigManager>(new ConfigManager());
    }
    return *instance;
}

void ConfigManager::initialize(const std::string& configFilePath) {
    auto& manager = getInstance();
    manager.configFilePath = manager.expandPath(configFilePath);
    manager.load();
}

void ConfigManager::setDefaults() {
    config = VerseLinkConfig(); // Use default values from struct
}

std::string ConfigManager::expandPath(const std::string& path) {
    // For now, just return the path as-is
    // Could be expanded to handle environment variables, relative paths, etc.
    return path;
}

bool ConfigManager::load() {
    if (configFilePath.empty()) {
        LOG_WARNING("No config file path specified, using defaults");
        return false;
    }
    
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        LOG_WARNING("Config file not found: " + configFilePath + ", creating with defaults");
        save(); // Create default config file
        return false;
    }
    
    try {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();
        
        // Simple JSON-like parsing (basic implementation)
        // In a production system, you'd use a proper JSON library
        
        // Parse bible version
        size_t pos = content.find("\"bibleVersion\"");
        if (pos != std::string::npos) {
            size_t start = content.find("\"", pos + 15) + 1;
            size_t end = content.find("\"", start);
            if (end != std::string::npos) {
                config.bibleVersion = content.substr(start, end - start);
            }
        }
        
        // Parse debug mode
        pos = content.find("\"debugMode\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.debugMode = (value.find("true") != std::string::npos);
            }
        }
        
        // Parse logging enabled
        pos = content.find("\"enableLogging\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.enableLogging = (value.find("true") != std::string::npos);
            }
        }
        
        // Parse include reference
        pos = content.find("\"includeReferenceInReplacement\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.includeReferenceInReplacement = (value.find("true") != std::string::npos);
            }
        }
        
        // Parse replacement format
        pos = content.find("\"replacementFormat\"");
        if (pos != std::string::npos) {
            size_t start = content.find("\"", pos + 20) + 1;
            size_t end = content.find("\"", start);
            if (end != std::string::npos) {
                config.replacementFormat = content.substr(start, end - start);
            }
        }
        
        // Parse prefer direct selection
        pos = content.find("\"preferDirectSelection\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.preferDirectSelection = (value.find("true") != std::string::npos);
            }
        }
        
        // Parse use existing clipboard
        pos = content.find("\"useExistingClipboard\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.useExistingClipboard = (value.find("true") != std::string::npos);
            }
        }
        
        // Parse icon path
        pos = content.find("\"iconPath\"");
        if (pos != std::string::npos) {
            size_t start = content.find("\"", pos + 11) + 1;
            size_t end = content.find("\"", start);
            if (end != std::string::npos) {
                config.iconPath = content.substr(start, end - start);
            }
        }
        
        // Parse verse formatting options
        pos = content.find("\"includeVerseNumbers\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.includeVerseNumbers = (value.find("true") != std::string::npos);
            }
        }
        
        pos = content.find("\"newLineBetweenChapters\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.newLineBetweenChapters = (value.find("true") != std::string::npos);
            }
        }
        
        pos = content.find("\"newLineBetweenBooks\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.newLineBetweenBooks = (value.find("true") != std::string::npos);
            }
        }
        
        pos = content.find("\"referenceOnFirstLine\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.referenceOnFirstLine = (value.find("true") != std::string::npos);
            }
        }

        pos = content.find("\"dynamicReference\"");
        if (pos != std::string::npos) {
            size_t start = content.find(":", pos) + 1;
            size_t end = content.find(",", start);
            if (end == std::string::npos) end = content.find("}", start);
            if (end != std::string::npos) {
                std::string value = content.substr(start, end - start);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                config.dynamicReference = (value.find("true") != std::string::npos);
            }
        }
        
        LOG_INFO("Configuration loaded successfully from: " + configFilePath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error parsing config file: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::save() {
    if (configFilePath.empty()) {
        LOG_ERROR("No config file path specified for saving");
        return false;
    }
    
    try {
        std::ofstream file(configFilePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file for writing: " + configFilePath);
            return false;
        }
        
        // Helper function to sanitize strings for JSON
        auto sanitizeJsonString = [](const std::string& input) -> std::string {
            std::string result;
            result.reserve(input.length() * 2); // Reserve space for potential escaping
            
            for (char c : input) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    case '\0': break; // Skip null characters entirely
                    default: result += c; break;
                }
            }
            return result;
        };
        
        // Simple JSON output with proper escaping
        file << "{\n";
        file << "  \"bibleVersion\": \"" << sanitizeJsonString(config.bibleVersion) << "\",\n";
        file << "  \"bibleDataPath\": \"" << sanitizeJsonString(config.bibleDataPath) << "\",\n";
        file << "  \"hotkeyModifiers\": " << config.hotkeyModifiers << ",\n";
        file << "  \"hotkeyVirtualKey\": " << config.hotkeyVirtualKey << ",\n";
        file << "  \"enableLogging\": " << (config.enableLogging ? "true" : "false") << ",\n";
        file << "  \"enableFileLogging\": " << (config.enableFileLogging ? "true" : "false") << ",\n";
        file << "  \"enableConsoleLogging\": " << (config.enableConsoleLogging ? "true" : "false") << ",\n";
        file << "  \"logFilePath\": \"" << sanitizeJsonString(config.logFilePath) << "\",\n";
        file << "  \"logLevel\": " << config.logLevel << ",\n";
        file << "  \"debugMode\": " << (config.debugMode ? "true" : "false") << ",\n";
        file << "  \"includeReferenceInReplacement\": " << (config.includeReferenceInReplacement ? "true" : "false") << ",\n";
        file << "  \"replacementFormat\": \"" << sanitizeJsonString(config.replacementFormat) << "\",\n";
        file << "  \"preferDirectSelection\": " << (config.preferDirectSelection ? "true" : "false") << ",\n";
        file << "  \"useExistingClipboard\": " << (config.useExistingClipboard ? "true" : "false") << ",\n";
        file << "  \"iconPath\": \"" << sanitizeJsonString(config.iconPath) << "\",\n";
        file << "  \"showNotifications\": " << (config.showNotifications ? "true" : "false") << ",\n";
        file << "  \"notificationDurationMs\": " << config.notificationDurationMs << ",\n";
        file << "  \"maxConcurrentTasks\": " << config.maxConcurrentTasks << ",\n";
        file << "  \"taskTimeoutMs\": " << config.taskTimeoutMs << ",\n";
        // Verse formatting options
        file << "  \"includeVerseNumbers\": " << (config.includeVerseNumbers ? "true" : "false") << ",\n";
        file << "  \"newLineBetweenChapters\": " << (config.newLineBetweenChapters ? "true" : "false") << ",\n";
        file << "  \"newLineBetweenBooks\": " << (config.newLineBetweenBooks ? "true" : "false") << ",\n";
        file << "  \"referenceOnFirstLine\": " << (config.referenceOnFirstLine ? "true" : "false") << ",\n";
        file << "  \"dynamicReference\": " << (config.dynamicReference ? "true" : "false") << "\n";
        file << "}\n";
        
        file.close();
        LOG_INFO("Configuration saved to: " + configFilePath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error saving config file: " + std::string(e.what()));
        return false;
    }
}

// Getters implementation already in header

// Setters
void ConfigManager::setBibleVersion(const std::string& version) {
    config.bibleVersion = version;
    notifySettingsChanged();
}

void ConfigManager::setBibleDataPath(const std::string& path) {
    config.bibleDataPath = path;
    notifySettingsChanged();
}

void ConfigManager::setHotkey(int modifiers, int virtualKey) {
    config.hotkeyModifiers = modifiers;
    config.hotkeyVirtualKey = virtualKey;
    notifySettingsChanged();
}

void ConfigManager::setLoggingEnabled(bool enabled) {
    config.enableLogging = enabled;
    notifySettingsChanged();
}

void ConfigManager::setDebugMode(bool enabled) {
    config.debugMode = enabled;
    notifySettingsChanged();
}

void ConfigManager::setIncludeReferenceInReplacement(bool include) {
    config.includeReferenceInReplacement = include;
    notifySettingsChanged();
}

void ConfigManager::setReplacementFormat(const std::string& format) {
    config.replacementFormat = format;
    notifySettingsChanged();
}

void ConfigManager::setPreferDirectSelection(bool prefer) {
    config.preferDirectSelection = prefer;
    notifySettingsChanged();
}

void ConfigManager::setUseExistingClipboard(bool use) {
    config.useExistingClipboard = use;
    notifySettingsChanged();
}

void ConfigManager::setIconPath(const std::string& iconPath) {
    config.iconPath = iconPath;
    notifySettingsChanged();
}

void ConfigManager::setLogFilePath(const std::string& logFilePath) {
    config.logFilePath = logFilePath;
    notifySettingsChanged();
}

// Verse formatting setters
void ConfigManager::setIncludeVerseNumbers(bool include) {
    config.includeVerseNumbers = include;
    notifySettingsChanged();
}

void ConfigManager::setNewLineBetweenChapters(bool newLine) {
    config.newLineBetweenChapters = newLine;
    notifySettingsChanged();
}

void ConfigManager::setNewLineBetweenBooks(bool newLine) {
    config.newLineBetweenBooks = newLine;
    notifySettingsChanged();
}

void ConfigManager::setReferenceOnFirstLine(bool referenceFirst) {
    config.referenceOnFirstLine = referenceFirst;
    notifySettingsChanged();
}

void ConfigManager::setDynamicReference(bool dynamic) {
    config.dynamicReference = dynamic;
    notifySettingsChanged();
}

void ConfigManager::notifySettingsChanged() {
    if (settingsChangeCallback && !batchUpdate) {
        settingsChangeCallback();
    }
}

void ConfigManager::setSettingsChangeCallback(SettingsChangeCallback callback) {
    settingsChangeCallback = callback;
}

void ConfigManager::beginBatchUpdate() {
    batchUpdate = true;
}

void ConfigManager::endBatchUpdate() {
    batchUpdate = false;
    if (settingsChangeCallback) {
        settingsChangeCallback();
    }
}
