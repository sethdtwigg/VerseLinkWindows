#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
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

namespace {
    // Returns the position of the first character of a key's JSON value,
    // i.e. just past the ':' following "<key>". npos if the key is absent/malformed.
    size_t FindValueStart(const std::string& json, const std::string& key) {
        std::string token = "\"" + key + "\"";
        size_t pos = json.find(token);
        if (pos == std::string::npos) return std::string::npos;
        pos = json.find(':', pos + token.size());
        if (pos == std::string::npos) return std::string::npos;
        ++pos;
        while (pos < json.size() && isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        return pos < json.size() ? pos : std::string::npos;
    }

    bool ExtractString(const std::string& json, const std::string& key, std::string& out) {
        size_t start = FindValueStart(json, key);
        if (start == std::string::npos || json[start] != '"') return false;
        std::string result;
        for (size_t i = start + 1; i < json.size(); ++i) {
            char c = json[i];
            if (c == '\\' && i + 1 < json.size()) {
                char next = json[++i];
                switch (next) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case 'r':  result += '\r'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    default:   result += next; break;
                }
            } else if (c == '"') {
                out = result;
                return true;
            } else {
                result += c;
            }
        }
        return false; // unterminated string
    }

    bool ExtractBool(const std::string& json, const std::string& key, bool& out) {
        size_t start = FindValueStart(json, key);
        if (start == std::string::npos) return false;
        if (json.compare(start, 4, "true") == 0) { out = true; return true; }
        if (json.compare(start, 5, "false") == 0) { out = false; return true; }
        return false;
    }

    bool ExtractInt(const std::string& json, const std::string& key, int& out) {
        size_t start = FindValueStart(json, key);
        if (start == std::string::npos) return false;
        try {
            size_t consumed = 0;
            int value = std::stoi(json.substr(start), &consumed);
            // Accept only if something numeric was actually consumed
            if (consumed == 0 ||
                (json[start] != '-' && json[start] != '+' && !isdigit(static_cast<unsigned char>(json[start])))) {
                return false;
            }
            out = value;
            return true;
        } catch (...) {
            return false;
        }
    }
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

        // Apply parsed values atomically with respect to other threads
        {
            std::lock_guard<std::mutex> lock(configMutex);

        // Bible settings
        ExtractString(content, "bibleVersion", config.bibleVersion);
        ExtractString(content, "bibleDataPath", config.bibleDataPath);

        // Hotkey settings
        ExtractInt(content, "hotkeyModifiers", config.hotkeyModifiers);
        ExtractInt(content, "hotkeyVirtualKey", config.hotkeyVirtualKey);

        // Logging settings
        ExtractBool(content, "enableLogging", config.enableLogging);
        ExtractBool(content, "enableFileLogging", config.enableFileLogging);
        ExtractBool(content, "enableConsoleLogging", config.enableConsoleLogging);
        ExtractString(content, "logFilePath", config.logFilePath);
        ExtractInt(content, "logLevel", config.logLevel);

        // Application settings
        ExtractBool(content, "debugMode", config.debugMode);
        ExtractBool(content, "includeReferenceInReplacement", config.includeReferenceInReplacement);
        ExtractString(content, "replacementFormat", config.replacementFormat);

        // Text selection settings
        ExtractBool(content, "preferDirectSelection", config.preferDirectSelection);
        ExtractBool(content, "useExistingClipboard", config.useExistingClipboard);

        // UI / icon settings
        ExtractString(content, "iconPath", config.iconPath);
        ExtractBool(content, "showNotifications", config.showNotifications);
        ExtractInt(content, "notificationDurationMs", config.notificationDurationMs);

        // Performance settings
        ExtractInt(content, "maxConcurrentTasks", config.maxConcurrentTasks);
        ExtractInt(content, "taskTimeoutMs", config.taskTimeoutMs);

        // Verse formatting options
        ExtractBool(content, "includeVerseNumbers", config.includeVerseNumbers);
        ExtractBool(content, "newLineBetweenChapters", config.newLineBetweenChapters);
        ExtractBool(content, "newLineBetweenBooks", config.newLineBetweenBooks);
        ExtractBool(content, "referenceOnFirstLine", config.referenceOnFirstLine);
        ExtractBool(content, "dynamicReference", config.dynamicReference);
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
        // Snapshot the config under lock, then write without holding it
        VerseLinkConfig snapshot;
        {
            std::lock_guard<std::mutex> lock(configMutex);
            snapshot = config;
        }
        const VerseLinkConfig& config = snapshot;

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

// Thread-safe getters
VerseLinkConfig ConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config;
}

std::string ConfigManager::getBibleVersion() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.bibleVersion;
}

std::string ConfigManager::getBibleDataPath() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.bibleDataPath;
}

int ConfigManager::getHotkeyModifiers() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.hotkeyModifiers;
}

int ConfigManager::getHotkeyVirtualKey() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.hotkeyVirtualKey;
}

bool ConfigManager::isLoggingEnabled() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.enableLogging;
}

bool ConfigManager::isDebugMode() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.debugMode;
}

bool ConfigManager::includeReferenceInReplacement() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.includeReferenceInReplacement;
}

std::string ConfigManager::getReplacementFormat() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.replacementFormat;
}

std::string ConfigManager::getLogFilePath() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.logFilePath;
}

std::string ConfigManager::getIconPath() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.iconPath;
}

bool ConfigManager::preferDirectSelection() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.preferDirectSelection;
}

bool ConfigManager::useExistingClipboard() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.useExistingClipboard;
}

bool ConfigManager::includeVerseNumbers() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.includeVerseNumbers;
}

bool ConfigManager::newLineBetweenChapters() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.newLineBetweenChapters;
}

bool ConfigManager::newLineBetweenBooks() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.newLineBetweenBooks;
}

bool ConfigManager::referenceOnFirstLine() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.referenceOnFirstLine;
}

bool ConfigManager::dynamicReference() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config.dynamicReference;
}

// Setters - mutate under lock, then notify with the lock released so the
// callback may safely call getters.
void ConfigManager::setBibleVersion(const std::string& version) {
    { std::lock_guard<std::mutex> lock(configMutex); config.bibleVersion = version; }
    notifySettingsChanged();
}

void ConfigManager::setBibleDataPath(const std::string& path) {
    { std::lock_guard<std::mutex> lock(configMutex); config.bibleDataPath = path; }
    notifySettingsChanged();
}

void ConfigManager::setHotkey(int modifiers, int virtualKey) {
    { std::lock_guard<std::mutex> lock(configMutex); config.hotkeyModifiers = modifiers; config.hotkeyVirtualKey = virtualKey; }
    notifySettingsChanged();
}

void ConfigManager::setLoggingEnabled(bool enabled) {
    { std::lock_guard<std::mutex> lock(configMutex); config.enableLogging = enabled; }
    notifySettingsChanged();
}

void ConfigManager::setDebugMode(bool enabled) {
    { std::lock_guard<std::mutex> lock(configMutex); config.debugMode = enabled; }
    notifySettingsChanged();
}

void ConfigManager::setIncludeReferenceInReplacement(bool include) {
    { std::lock_guard<std::mutex> lock(configMutex); config.includeReferenceInReplacement = include; }
    notifySettingsChanged();
}

void ConfigManager::setReplacementFormat(const std::string& format) {
    { std::lock_guard<std::mutex> lock(configMutex); config.replacementFormat = format; }
    notifySettingsChanged();
}

void ConfigManager::setPreferDirectSelection(bool prefer) {
    { std::lock_guard<std::mutex> lock(configMutex); config.preferDirectSelection = prefer; }
    notifySettingsChanged();
}

void ConfigManager::setUseExistingClipboard(bool use) {
    { std::lock_guard<std::mutex> lock(configMutex); config.useExistingClipboard = use; }
    notifySettingsChanged();
}

void ConfigManager::setIconPath(const std::string& iconPath) {
    { std::lock_guard<std::mutex> lock(configMutex); config.iconPath = iconPath; }
    notifySettingsChanged();
}

void ConfigManager::setLogFilePath(const std::string& logFilePath) {
    { std::lock_guard<std::mutex> lock(configMutex); config.logFilePath = logFilePath; }
    notifySettingsChanged();
}

// Verse formatting setters
void ConfigManager::setIncludeVerseNumbers(bool include) {
    { std::lock_guard<std::mutex> lock(configMutex); config.includeVerseNumbers = include; }
    notifySettingsChanged();
}

void ConfigManager::setNewLineBetweenChapters(bool newLine) {
    { std::lock_guard<std::mutex> lock(configMutex); config.newLineBetweenChapters = newLine; }
    notifySettingsChanged();
}

void ConfigManager::setNewLineBetweenBooks(bool newLine) {
    { std::lock_guard<std::mutex> lock(configMutex); config.newLineBetweenBooks = newLine; }
    notifySettingsChanged();
}

void ConfigManager::setReferenceOnFirstLine(bool referenceFirst) {
    { std::lock_guard<std::mutex> lock(configMutex); config.referenceOnFirstLine = referenceFirst; }
    notifySettingsChanged();
}

void ConfigManager::setDynamicReference(bool dynamic) {
    { std::lock_guard<std::mutex> lock(configMutex); config.dynamicReference = dynamic; }
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
