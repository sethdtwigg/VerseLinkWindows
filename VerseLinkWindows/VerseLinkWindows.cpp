#include "VerseLinkWindows.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <mutex>
#include <atomic>
#include <condition_variable>

// Global variable definitions
std::atomic<bool> Logging{false};
std::atomic<bool> Debugging{true};

// Bible version string shared across threads; guarded because std::string
// cannot be safely read while another thread writes it.
static std::string g_bibleVersion;
static std::mutex g_bibleVersionMutex;

std::string GetBibleVersionSetting() {
    std::lock_guard<std::mutex> lock(g_bibleVersionMutex);
    return g_bibleVersion;
}

void SetBibleVersionSetting(const std::string& version) {
    std::lock_guard<std::mutex> lock(g_bibleVersionMutex);
    g_bibleVersion = version;
}

// Public wrapper so the settings dialog can queue work from its modal loop.
static void QueueVerseLinkTask();
void QueueHotkeyTask() {
    QueueVerseLinkTask();
}

// Thread synchronization globals
// A single persistent worker thread executes verse tasks; the UI thread only
// queues work and is never blocked by clipboard/UIA sleeps.
static std::thread g_workerThread;
static std::mutex g_taskMutex;
static std::condition_variable g_taskCV;
static std::atomic<bool> g_shouldExit(false);
static std::atomic<bool> g_taskPending(false);
static DWORD g_mainThreadId = 0;

// Hotkey registration state (for live re-registration when settings change)
static int g_registeredModifiers = 0;
static int g_registeredVirtualKey = 0;

static void Log(const std::string& message) {
    if (Logging) {
        LOG_INFO(message);
    }
}

static bool RegisterAppHotkey(int modifiers, int virtualKey) {
    UnregisterHotKey(nullptr, MY_HOTKEY_ID);
    // MOD_NOREPEAT suppresses auto-repeat when the combo is held down.
    if (!RegisterHotKey(nullptr, MY_HOTKEY_ID, modifiers | MOD_NOREPEAT, virtualKey)) {
        LOG_ERROR("Failed to register hotkey (modifiers=" + std::to_string(modifiers) +
                  ", virtualKey=" + std::to_string(virtualKey) +
                  "). It may be in use by another application.");
        g_registeredModifiers = 0;
        g_registeredVirtualKey = 0;
        return false;
    }
    g_registeredModifiers = modifiers;
    g_registeredVirtualKey = virtualKey;
    return true;
}

// Settings update callback
void OnSettingsChanged() {
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // Update global variables from config
    SetBibleVersionSetting(config.bibleVersion);
    Logging = config.enableLogging;
    Debugging = config.debugMode;
    
    // Re-register the hotkey if it changed
    if ((g_registeredModifiers != config.hotkeyModifiers ||
         g_registeredVirtualKey != config.hotkeyVirtualKey) &&
        !RegisterAppHotkey(config.hotkeyModifiers, config.hotkeyVirtualKey)) {
        LOG_WARNING("Keeping previously registered hotkey");
    }

    // Apply logger changes (level/outputs); reopen file if its path changed
    Logger::initialize(config.logFilePath,
                       static_cast<LogLevel>(config.logLevel),
                       config.enableConsoleLogging, config.enableFileLogging);
    
    LOG_INFO("Settings updated - Bible version: " + config.bibleVersion + ", Logging: " + (Logging ? "enabled" : "disabled") + ", Debug: " + (Debugging ? "enabled" : "disabled"));
}

void VerseLinkTask() {
    try {
        LOG_INFO("Starting VerseLink task");
        
        // Snapshot settings so a concurrent settings change cannot tear values
        // mid-task. getConfig() returns an atomic copy.
        const VerseLinkConfig config = ConfigManager::getInstance().getConfig();
        std::string bibleVersion = GetBibleVersionSetting();
        
        // Get selected text
        ClipboardInterface ci;
        std::string selectedText = ci.GetSelectedText();
        
        LOG_INFO("Selected text: '" + selectedText + "'");
        LOG_INFO("ClipboardInterface log:\n" + ci.GetLog());
        
        if (!selectedText.empty()) {
            // Retrieve verse
            LOG_INFO("Creating VerseRetrieveInterface with: '" + selectedText + "' and version: '" + bibleVersion + "'");
            std::unique_ptr<VerseRetrieveInterface> vri(new VerseRetrieveInterface(selectedText, bibleVersion));
            LOG_INFO("VerseRetrieveInterface created. Log:\n" + vri->GetLog());
            
            if (vri->GetVerseText()) {
                LOG_INFO("Successfully retrieved verse text");

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
    
    LOG_INFO("VerseLink task completed");
}

// Worker thread body: waits for queued tasks or shutdown.
static void VerseLinkWorkerProc() {
    while (true) {
        std::unique_lock<std::mutex> lock(g_taskMutex);
        g_taskCV.wait(lock, [] { return g_shouldExit.load() || g_taskPending.load(); });
        if (g_shouldExit) {
            break;
        }
        g_taskPending = false;
        lock.unlock();
        VerseLinkTask();
    }
}

// Called from the UI thread when the hotkey fires.
static void QueueVerseLinkTask() {
    bool alreadyPending = g_taskPending.exchange(true);
    if (alreadyPending) {
        LOG_INFO("Task already queued, coalescing hotkey press");
        return;
    }
    g_taskCV.notify_one();
}

bool RunVerseLink(HWND hwnd, SystemTray* systemTray) {
    try {
        LOG_INFO("Entering message loop");
        MSG msg;
        
        while (!g_shouldExit) {
            int result = GetMessage(&msg, nullptr, 0, 0);
            if (result <= 0) {
                if (result == -1) {
                    LOG_ERROR("GetMessage failed, exiting message loop");
                }
                break;
            }

            if (msg.message == WM_HOTKEY && msg.wParam == MY_HOTKEY_ID) {
                LOG_INFO("Hotkey pressed!");
                QueueVerseLinkTask();
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
        
    // Update global variables from config
    SetBibleVersionSetting(config.bibleVersion);
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
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT ||
        ctrlType == CTRL_CLOSE_EVENT) {
        LOG_INFO("Received shutdown signal, exiting gracefully...");
        g_shouldExit = true;
        g_taskCV.notify_all();
        // Wake the main thread's GetMessage() so the message loop can exit.
        PostThreadMessage(g_mainThreadId, WM_QUIT, 0, 0);
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
    g_mainThreadId = GetCurrentThreadId();

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
    
    // Hide console if not debugging (must hide before freeing the console,
    // otherwise GetConsoleWindow() returns NULL)
    if (!Debugging) {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
        FreeConsole();
    }
    
    // Start the persistent worker thread that executes verse tasks
    g_workerThread = std::thread(VerseLinkWorkerProc);
    
    LOG_INFO("VerseLink starting up...");
    LOG_INFO("Configuration loaded successfully");
    LOG_INFO("Bible version: " + config.bibleVersion);
    LOG_INFO("Logging: " + std::string(config.enableLogging ? "enabled" : "disabled"));
    LOG_INFO("Debug mode: " + std::string(config.debugMode ? "enabled" : "disabled"));
    
    // Register hotkey from configuration
    if (!RegisterAppHotkey(config.hotkeyModifiers, config.hotkeyVirtualKey)) {
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
    
    // Stop the worker thread and wait for any in-flight task to finish so we
    // never exit while it is still touching globals or the clipboard.
    g_shouldExit = true;
    g_taskCV.notify_all();
    if (g_workerThread.joinable()) {
        g_workerThread.join();
    }
    
    LOG_INFO("VerseLink shutdown complete");
    return success ? 0 : 1;
}
