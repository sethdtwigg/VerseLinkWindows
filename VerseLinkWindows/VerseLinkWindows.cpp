#include "VerseLinkWindows.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <mutex>
#include <atomic>
#include <condition_variable>

// Global variable definitions
std::string BV;
bool Logging = false;
bool Debugging = true;

// Settings update callback
void OnSettingsChanged() {
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // Update global variables from config
    BV = config.bibleVersion;
    Logging = config.enableLogging;
    Debugging = config.debugMode;
    
    LOG_INFO("Settings updated - Bible version: " + BV + ", Logging: " + (Logging ? "enabled" : "disabled") + ", Debug: " + (Debugging ? "enabled" : "disabled"));
}

// Thread synchronization globals
std::mutex g_threadMutex;
std::condition_variable g_threadCV;
std::atomic<bool> g_shouldExit(false);
std::atomic<bool> g_processingHotkey(false);

void Log(const std::string& message) {
    if (Logging) {
        LOG_INFO(message);
    }
}

void VerseLinkTask() {
    // Prevent multiple simultaneous executions
    if (g_processingHotkey.exchange(true)) {
        LOG_WARNING("VerseLink task already running, skipping...");
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_threadMutex);
    
    try {
        LOG_INFO("Starting VerseLink task");
        g_processingHotkey = true;
        
        // Get selected text
        ClipboardInterface ci;
        std::string selectedText = ci.GetSelectedText();
        
        LOG_INFO("Selected text: '" + selectedText + "'");
        LOG_INFO("ClipboardInterface log:\n" + ci.GetLog());
        
        if (!selectedText.empty()) {
            // Retrieve verse
            LOG_INFO("Creating VerseRetrieveInterface with: '" + selectedText + "' and version: '" + BV + "'");
            std::unique_ptr<VerseRetrieveInterface> vri(new VerseRetrieveInterface(selectedText, BV));
            LOG_INFO("VerseRetrieveInterface created. Log:\n" + vri->GetLog());
            
            if (vri->GetVerseText()) {
                LOG_INFO("Successfully retrieved verse text");
                const auto& config = ConfigManager::getInstance().getConfig();

                std::string reference = vri->ReferenceText.empty() ? selectedText : vri->ReferenceText;
                std::string text = vri->VerseText;

                std::string replacementText = config.replacementFormat;
                size_t pos = replacementText.find("{reference}");
                if (pos != std::string::npos) {
                    replacementText.replace(pos, std::string("{reference}").size(), reference);
                }
                pos = replacementText.find("{text}");
                if (pos != std::string::npos) {
                    replacementText.replace(pos, std::string("{text}").size(), text);
                }

                if (!config.includeReferenceInReplacement) {
                    replacementText = text;
                } else if (config.referenceOnFirstLine && !reference.empty()) {
                    if (!text.empty() && text.front() == '\n') {
                        replacementText = reference + text;
                    } else {
                        replacementText = reference + "\n" + text;
                    }
                }
                LOG_INFO("Replacement text: '" + replacementText + "'");
                
                if (ci.ReplaceSelectedText(replacementText)) {
                    LOG_INFO("Successfully replaced selected text");
                } else {
                    LOG_ERROR("Failed to replace selected text: " + ci.GetLastError());
                }
                LOG_INFO("Replacement log:\n" + ci.GetLog());
            } else {
                LOG_WARNING("Failed to retrieve verse text: " + vri->LastError);
                LOG_INFO("VerseRetrieveInterface log:\n" + vri->GetLog());
            }
        } else {
            LOG_DEBUG("No text selected or text is empty");
            LOG_INFO("Selection log:\n" + ci.GetLog());
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in VerseLinkTask: " + std::string(e.what()));
    }
    catch (...) {
        LOG_ERROR("Unknown exception in VerseLinkTask");
    }
    
    g_processingHotkey = false;
    LOG_INFO("VerseLink task completed");
}

bool RunVerseLink(HWND hwnd, SystemTray* systemTray) {
    try {
        LOG_INFO("Entering message loop");
        MSG msg;
        
        while (!g_shouldExit && GetMessage(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_HOTKEY && msg.wParam == MY_HOTKEY_ID) {
                LOG_INFO("Hotkey pressed!");
                
                // Handle hotkey in a separate thread to avoid blocking message loop
                std::thread hotkeyThread([]() {
                    VerseLinkTask();
                });
                hotkeyThread.detach();
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        LOG_INFO("Exiting message loop");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in RunVerseLink: " + std::string(e.what()));
        return false;
    }
}

bool GetConfiguration() {
    try {
        // Initialize configuration manager
        ConfigManager::initialize("config.json");
        
        // Register settings change callback
        ConfigManager::getInstance().setSettingsChangeCallback(OnSettingsChanged);
        
        const auto& config = ConfigManager::getInstance().getConfig();
        
        // Set global variables from config
        BV = config.bibleVersion;
        Logging = config.enableLogging;
        Debugging = config.debugMode;
        
        // Initialize logger with config settings
        LogLevel logLevel = static_cast<LogLevel>(config.logLevel);
        Logger::initialize(config.logFilePath, logLevel, 
                         config.enableConsoleLogging, config.enableFileLogging);
        
        LOG_INFO("Configuration loaded from config.json");
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load configuration: " << e.what() << std::endl;
        return false;
    }
}

// Console control handler for graceful shutdown
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        LOG_INFO("Received shutdown signal, exiting gracefully...");
        g_shouldExit = true;
        g_threadCV.notify_all();
        return TRUE;
    }
    return FALSE;
}

// Window procedure for handling system tray messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Get system tray instance from window data
    SystemTray* systemTray = reinterpret_cast<SystemTray*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (systemTray && uMsg == WM_TRAYICON) {
        systemTray->HandleMessage(uMsg, wParam, lParam);
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main()
{
    // Load configuration first (this also initializes the logger)
    if (!GetConfiguration()) {
        std::cerr << "Failed to load configuration, exiting..." << std::endl;
        return 1;
    }
    
    // Get configuration for hotkey
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // Create a hidden window for system tray messages
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"VerseLinkHiddenWindow";
    
    if (!RegisterClass(&wc)) {
        LOG_ERROR("Failed to register window class");
        return 1;
    }
    
    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"VerseLink",
        0,
        0, 0, 0, 0,
        nullptr, nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!hwnd) {
        LOG_ERROR("Failed to create window");
        return 1;
    }
    
    // Initialize system tray
    std::unique_ptr<SystemTray> systemTray(new SystemTray(hwnd));
    if (!systemTray->Initialize()) {
        LOG_ERROR("Failed to initialize system tray");
    } else {
        // Set custom icon if configured BEFORE showing the tray
        auto& configManager = ConfigManager::getInstance();
        std::string iconPath = configManager.getIconPath();
        if (!iconPath.empty()) {
            systemTray->SetCustomIcon(iconPath);
        } else {
            LOG_INFO("No custom icon path configured, using default icon");
        }
        
        systemTray->Show();
        systemTray->UpdateTooltip("VerseLink - Press Ctrl+Alt+L to insert verse");
        LOG_INFO("System tray initialized");
        
        // Store system tray pointer in window data
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(systemTray.get()));
    }
    
    // Set up console control handler
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    // Hide console if not debugging
    if (!Debugging) {
        FreeConsole();
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
    
    LOG_INFO("VerseLink starting up...");
    LOG_INFO("Configuration loaded successfully");
    LOG_INFO("Bible version: " + config.bibleVersion);
    LOG_INFO("Logging: " + std::string(config.enableLogging ? "enabled" : "disabled"));
    LOG_INFO("Debug mode: " + std::string(config.debugMode ? "enabled" : "disabled"));
    
    // Register hotkey from configuration
    if (!RegisterHotKey(nullptr, MY_HOTKEY_ID, config.hotkeyModifiers, config.hotkeyVirtualKey)) {
        LOG_ERROR("Failed to register hotkey");
        return 1;
    }
    
    LOG_INFO("Hotkey registered successfully");
    LOG_INFO("VerseLink is now running in the background");
    
    // Run the main message loop
    bool success = RunVerseLink(hwnd, systemTray.get());
    
    // Cleanup
    LOG_INFO("Cleaning up...");
    UnregisterHotKey(nullptr, MY_HOTKEY_ID);
    
    // Wait for any remaining threads to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    LOG_INFO("VerseLink shutdown complete");
    return success ? 0 : 1;
}
