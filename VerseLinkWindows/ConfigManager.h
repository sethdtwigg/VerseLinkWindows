#pragma once
#ifndef ConfigManager_H
#define ConfigManager_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <windows.h>

struct VerseLinkConfig {
    // Bible settings
    std::string bibleVersion = "KJV.xml";
    std::string bibleDataPath = "";
    
    // Hotkey settings
    int hotkeyModifiers = MOD_CONTROL | MOD_ALT;
    int hotkeyVirtualKey = 'L'; // 'L' key
    
    // Logging settings
    bool enableLogging = true;
    bool enableFileLogging = true;
    bool enableConsoleLogging = true;
    std::string logFilePath = "verselink.log";
    int logLevel = 1; // INFO level
    
    // Application settings
    bool debugMode = false;
    bool includeReferenceInReplacement = true;
    std::string replacementFormat = "{reference} {text}"; // Format for replacement text
    
    // Verse formatting settings
    bool includeVerseNumbers = false; // Include verse numbers before each verse
    bool newLineBetweenChapters = false; // Add new line between chapters
    bool newLineBetweenBooks = false; // Add new line between books
    bool referenceOnFirstLine = false; // Place reference on first line, verses on subsequent lines
    bool dynamicReference = false; // Shorten displayed reference to Book Chapter:StartVerse
    
    // Text selection settings
    bool preferDirectSelection = true; // Try UI Automation and edit controls first
    bool useExistingClipboard = true;  // Use existing clipboard content before sending Ctrl+C
    
    // UI settings
    bool showNotifications = false;
    int notificationDurationMs = 3000;
    
    // Icon settings
    std::string iconPath = "VLIcon.ico"; // Default to VLIcon.ico in same directory
    
    // Performance settings
    int maxConcurrentTasks = 1;
    int taskTimeoutMs = 5000;
};

class ConfigManager {
private:
    static std::unique_ptr<ConfigManager> instance;
    static std::mutex instanceMutex;
    
    VerseLinkConfig config;
    std::string configFilePath;
    bool batchUpdate = false; // Flag to prevent callbacks during batch updates
    
    // Forward declaration of callback type
    typedef void (*SettingsChangeCallback)();
    SettingsChangeCallback settingsChangeCallback = nullptr;
    
    ConfigManager();
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath);
    void setDefaults();
    std::string expandPath(const std::string& path);
    
public:
    static ConfigManager& getInstance();
    static void initialize(const std::string& configFilePath = "config.json");
    
    bool load();
    bool save();
    
    // Getters
    const VerseLinkConfig& getConfig() const { return config; }
    std::string getBibleVersion() const { return config.bibleVersion; }
    std::string getBibleDataPath() const { return config.bibleDataPath; }
    int getHotkeyModifiers() const { return config.hotkeyModifiers; }
    int getHotkeyVirtualKey() const { return config.hotkeyVirtualKey; }
    bool isLoggingEnabled() const { return config.enableLogging; }
    bool isDebugMode() const { return config.debugMode; }
    bool includeReferenceInReplacement() const { return config.includeReferenceInReplacement; }
    std::string getReplacementFormat() const { return config.replacementFormat; }
    std::string getLogFilePath() const { return config.logFilePath; }
    std::string getIconPath() const { return config.iconPath; }
    bool preferDirectSelection() const { return config.preferDirectSelection; }
    bool useExistingClipboard() const { return config.useExistingClipboard; }
    
    // Verse formatting getters
    bool includeVerseNumbers() const { return config.includeVerseNumbers; }
    bool newLineBetweenChapters() const { return config.newLineBetweenChapters; }
    bool newLineBetweenBooks() const { return config.newLineBetweenBooks; }
    bool referenceOnFirstLine() const { return config.referenceOnFirstLine; }
    bool dynamicReference() const { return config.dynamicReference; }
    
    // Setters
    void setBibleVersion(const std::string& version);
    void setBibleDataPath(const std::string& path);
    void setHotkey(int modifiers, int virtualKey);
    void setLoggingEnabled(bool enabled);
    void setDebugMode(bool enabled);
    void setIncludeReferenceInReplacement(bool include);
    void setReplacementFormat(const std::string& format);
    void setPreferDirectSelection(bool prefer);
    void setUseExistingClipboard(bool use);
    void setIconPath(const std::string& iconPath);
    void setLogFilePath(const std::string& logFilePath);
    
    // Verse formatting setters
    void setIncludeVerseNumbers(bool include);
    void setNewLineBetweenChapters(bool newLine);
    void setNewLineBetweenBooks(bool newLine);
    void setReferenceOnFirstLine(bool referenceFirst);
    void setDynamicReference(bool dynamic);
    
    // Settings change notification
    void notifySettingsChanged();
    void setSettingsChangeCallback(SettingsChangeCallback callback);
    
    // Batch update methods
    void beginBatchUpdate();
    void endBatchUpdate();
    
    ~ConfigManager() = default;
};

#endif
