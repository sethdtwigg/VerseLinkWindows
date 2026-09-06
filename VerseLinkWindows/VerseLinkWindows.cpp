#include "VerseLinkWindows.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "TaskQueue.h"
#include "VerseFormatter.h"
#include "SelfTest.h"
#include "StringExtensions.h"
#include "Version.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <filesystem>
#include <shellapi.h>

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
static TaskQueue g_taskQueue;
static std::atomic<bool> g_shouldExit(false);
static DWORD g_mainThreadId = 0;
static HWND g_mainWindow = nullptr;

// Directory holding config.json; relative log paths resolve against it so an
// installed copy writes its log beside its settings rather than into whatever
// the working directory happens to be.
static std::string g_configDirectory;

// Hotkey registration state (for live re-registration when settings change)
static bool g_hotkeyRegistered = false;
static int g_registeredModifiers = 0;
static int g_registeredVirtualKey = 0;

// An absolute logFilePath is honoured as-is; a relative one lands beside
// config.json. Left relative it would follow the working directory, so a
// shortcut-launched copy would scatter logs and the tray's "View Log" would
// read a different file than the one being written.
std::string ResolveLogPath(const std::string& configuredPath) {
    if (configuredPath.empty() || g_configDirectory.empty()) {
        return configuredPath;
    }

    const std::filesystem::path path(StringExtensions::Utf8ToWide(configuredPath));
    if (path.is_absolute()) {
        return configuredPath;
    }

    const std::filesystem::path base(StringExtensions::Utf8ToWide(g_configDirectory));
    return StringExtensions::WideToUtf8((base / path).wstring());
}

std::string DescribeHotkey(int modifiers, int virtualKey) {
    std::string description;
    if (modifiers & MOD_CONTROL) description += "Ctrl+";
    if (modifiers & MOD_ALT)     description += "Alt+";
    if (modifiers & MOD_SHIFT)   description += "Shift+";
    if (modifiers & MOD_WIN)     description += "Win+";

    const UINT mapped = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_CHAR) & 0x7FFF;
    if (mapped >= 0x20 && mapped < 0x7F) {
        description += static_cast<char>(mapped);
    } else {
        description += "VK" + std::to_string(virtualKey);
    }
    return description;
}

// Registers the hotkey, never at the cost of the one already working.
//
// RegisterHotKey is the only way to discover whether another application owns a
// combination, so the new one is probed under a scratch id first. Releasing the
// current registration up front (as this used to) meant a failed change left the
// app with no hotkey at all while logging that it had kept the old one.
static bool RegisterAppHotkey(int modifiers, int virtualKey) {
    const UINT flags = static_cast<UINT>(modifiers) | MOD_NOREPEAT;

    if (g_hotkeyRegistered &&
        modifiers == g_registeredModifiers &&
        virtualKey == g_registeredVirtualKey) {
        return true; // already registered to us
    }

    if (!RegisterHotKey(nullptr, PROBE_HOTKEY_ID, flags, virtualKey)) {
        LOG_ERROR("Failed to register hotkey " + DescribeHotkey(modifiers, virtualKey) +
                  " (modifiers=" + std::to_string(modifiers) +
                  ", virtualKey=" + std::to_string(virtualKey) +
                  "). It may be in use by another application.");
        return false;
    }
    UnregisterHotKey(nullptr, PROBE_HOTKEY_ID);

    const bool hadHotkey = g_hotkeyRegistered;
    const int previousModifiers = g_registeredModifiers;
    const int previousVirtualKey = g_registeredVirtualKey;

    if (hadHotkey) {
        UnregisterHotKey(nullptr, MY_HOTKEY_ID);
        g_hotkeyRegistered = false;
    }

    if (RegisterHotKey(nullptr, MY_HOTKEY_ID, flags, virtualKey)) {
        g_hotkeyRegistered = true;
        g_registeredModifiers = modifiers;
        g_registeredVirtualKey = virtualKey;
        return true;
    }

    LOG_ERROR("Failed to register hotkey " + DescribeHotkey(modifiers, virtualKey) +
              " after releasing the previous one");

    if (hadHotkey && RegisterHotKey(nullptr, MY_HOTKEY_ID,
                                    static_cast<UINT>(previousModifiers) | MOD_NOREPEAT,
                                    previousVirtualKey)) {
        g_hotkeyRegistered = true;
        g_registeredModifiers = previousModifiers;
        g_registeredVirtualKey = previousVirtualKey;
        LOG_WARNING("Restored the previously registered hotkey " +
                    DescribeHotkey(previousModifiers, previousVirtualKey));
    } else {
        g_registeredModifiers = 0;
        g_registeredVirtualKey = 0;
    }
    return false;
}

static void UpdateTrayTooltip() {
    if (!g_mainWindow) return;
    SystemTray* systemTray = reinterpret_cast<SystemTray*>(GetWindowLongPtr(g_mainWindow, GWLP_USERDATA));
    if (!systemTray) return;

    if (g_hotkeyRegistered) {
        systemTray->UpdateTooltip("VerseLink - Press " +
                                  DescribeHotkey(g_registeredModifiers, g_registeredVirtualKey) +
                                  " to insert verse");
    } else {
        systemTray->UpdateTooltip("VerseLink - no hotkey registered");
    }
}

// Settings update callback
void OnSettingsChanged() {
    const VerseLinkConfig config = ConfigManager::getInstance().getConfig();

    // Update global variables from config
    SetBibleVersionSetting(config.bibleVersion);
    Logging = config.enableLogging;
    Debugging = config.debugMode;

    // Re-register the hotkey if it changed
    if (!RegisterAppHotkey(config.hotkeyModifiers, config.hotkeyVirtualKey)) {
        LOG_WARNING("Hotkey unchanged; the requested combination could not be registered");
    }
    UpdateTrayTooltip();

    // Apply logger changes (level/outputs); reopen file if its path changed
    Logger::initialize(ResolveLogPath(config.logFilePath),
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

                const std::string reference = vri->ReferenceText.empty() ? selectedText : vri->ReferenceText;
                const std::string replacementText =
                    VerseFormatter::ComposeReplacementText(config, reference, vri->VerseText);

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
    while (g_taskQueue.WaitForTask()) {
        VerseLinkTask();
    }
}

// Called from the UI thread when the hotkey fires.
static void QueueVerseLinkTask() {
    if (!g_taskQueue.Post()) {
        LOG_INFO("Task already queued, coalescing hotkey press");
    }
}

// Signals the worker to stop and waits for any in-flight task, so we never exit
// while it is still touching globals or the clipboard. Safe to call more than
// once, and every exit path after the thread starts goes through it.
static void ShutdownWorker() {
    g_shouldExit = true;
    g_taskQueue.Shutdown();
    if (g_workerThread.joinable()) {
        g_workerThread.join();
    }
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
        // Settings live in %APPDATA%\VerseLink so an installed copy can write
        // them. Only fall back to the working directory if that folder is
        // unavailable, which is the old behaviour.
        std::string configPath = ConfigManager::userConfigPath();
        std::string migratedFrom;
        if (configPath.empty()) {
            configPath = "config.json";
        } else {
            // First run after an upgrade: carry settings over from the copy an
            // older version kept beside the exe or in the working directory.
            migratedFrom = ConfigManager::migrateLegacyConfig(
                configPath, ConfigManager::legacyConfigPaths());
        }

        g_configDirectory = StringExtensions::WideToUtf8(
            std::filesystem::path(StringExtensions::Utf8ToWide(configPath)).parent_path().wstring());

        // Initialize configuration manager
        ConfigManager::initialize(configPath);

        // Register settings change callback
        ConfigManager::getInstance().setSettingsChangeCallback(OnSettingsChanged);

        const VerseLinkConfig config = ConfigManager::getInstance().getConfig();

        // Update global variables from config
        SetBibleVersionSetting(config.bibleVersion);
        Logging = config.enableLogging;
        Debugging = config.debugMode;

        // Initialize logger with config settings
        LogLevel logLevel = static_cast<LogLevel>(config.logLevel);
        Logger::initialize(ResolveLogPath(config.logFilePath), logLevel,
                           config.enableConsoleLogging, config.enableFileLogging);

        LOG_INFO("Configuration loaded from " + configPath);
        if (!migratedFrom.empty()) {
            LOG_INFO("Migrated settings from a previous installation at " + migratedFrom);
        }
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
        g_taskQueue.Shutdown();
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

    switch (uMsg) {
        // An outside request to shut down: the installer closing us before it
        // replaces the exe, or Windows signing the user out. Restart Manager
        // sends WM_QUERYENDSESSION then WM_ENDSESSION to top-level windows, so
        // handling these is what lets an upgrade close VerseLink cleanly
        // instead of failing on a locked exe.
        //
        // WM_CLOSE previously fell through to DefWindowProc, which destroyed
        // the window but left the message loop and the worker thread running -
        // the process stayed alive with no window and no tray icon.
        case WM_QUERYENDSESSION:
            return TRUE; // yes, we can shut down

        case WM_CLOSE:
        case WM_ENDSESSION:
            LOG_INFO("Shutdown requested by the system or an installer");
            g_shouldExit = true;
            g_taskQueue.Shutdown();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Arguments as UTF-8, taken from the wide command line rather than main's argv.
// MSVC hands narrow argv over in the ANSI codepage, so a reference pasted with a
// real en dash reaches argv as '?' - which is precisely the input --verse exists
// to reproduce.
static std::vector<std::string> Utf8CommandLineArgs() {
    std::vector<std::string> args;

    int wideArgc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (!wideArgv) return args;

    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(StringExtensions::WideToUtf8(wideArgv[i]));
    }
    LocalFree(wideArgv);
    return args;
}

int main()
{
    // Headless modes run before any window, tray icon, hotkey or worker thread
    // exists, so CI can exercise the real lookup and formatting path with no GUI.
    const std::vector<std::string> args = Utf8CommandLineArgs();
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--selftest") {
            return SelfTest::Run();
        }
        if (args[i] == "--verse") {
            if (i + 1 >= args.size()) {
                std::cerr << "--verse requires a reference, e.g. --verse \"Romans 8:1-5\"" << std::endl;
                return 2;
            }
            return SelfTest::PrintVerse(args[i + 1]);
        }
        if (args[i] == "--help" || args[i] == "-h") {
            std::cout << "VerseLink\n"
                      << "  (no arguments)      run in the system tray\n"
                      << "  --selftest          run the headless end-to-end checks\n"
                      << "  --verse <reference> print the replacement text for a reference\n";
            return 0;
        }
        std::cerr << "Unknown argument: " << args[i] << " (try --help)" << std::endl;
        return 2;
    }

    // Held for the life of the process. Two jobs:
    //  - the installer names this mutex so it can tell VerseLink is running and
    //    ask the user to close it, instead of failing on a locked exe mid-upgrade;
    //  - it keeps a second copy from starting, which would otherwise add a second
    //    tray icon and fight over the same hotkey.
    // Named without a namespace prefix so it lives in the caller's session, which
    // is what a per-user install wants and what the installer looks for.
    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, VERSELINK_INSTANCE_MUTEX);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "VerseLink is already running (check the system tray)." << std::endl;
        CloseHandle(instanceMutex);
        return 0;
    }

    g_mainThreadId = GetCurrentThreadId();

    // Load configuration first (this also initializes the logger)
    if (!GetConfiguration()) {
        std::cerr << "Failed to load configuration, exiting..." << std::endl;
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }

    // Get configuration for hotkey
    const VerseLinkConfig config = ConfigManager::getInstance().getConfig();

    // Create a hidden window for system tray messages
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    // The installer finds this window by class name to ask VerseLink to close
    // before it replaces the exe (packaging\VerseLink.iss, AppWindowClass).
    // Renaming it breaks that, and updates would fail on a locked file.
    wc.lpszClassName = L"VerseLinkHiddenWindow";

    if (!RegisterClass(&wc)) {
        LOG_ERROR("Failed to register window class");
        if (instanceMutex) CloseHandle(instanceMutex);
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
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }
    g_mainWindow = hwnd;

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

    LOG_INFO("VerseLink starting up...");
    LOG_INFO("Configuration loaded successfully");
    LOG_INFO("Bible version: " + config.bibleVersion);
    LOG_INFO("Logging: " + std::string(config.enableLogging ? "enabled" : "disabled"));
    LOG_INFO("Debug mode: " + std::string(config.debugMode ? "enabled" : "disabled"));

    // Register the hotkey before starting the worker thread, so this failure
    // path has no joinable std::thread to trip over on the way out.
    if (!RegisterAppHotkey(config.hotkeyModifiers, config.hotkeyVirtualKey)) {
        LOG_ERROR("Failed to register hotkey");
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }
    UpdateTrayTooltip();

    LOG_INFO("Hotkey registered successfully: " +
             DescribeHotkey(config.hotkeyModifiers, config.hotkeyVirtualKey));
    LOG_INFO("VerseLink is now running in the background");

    // Start the persistent worker thread that executes verse tasks
    g_workerThread = std::thread(VerseLinkWorkerProc);

    // Run the main message loop
    bool success = RunVerseLink(hwnd, systemTray.get());

    // Cleanup
    LOG_INFO("Cleaning up...");
    UnregisterHotKey(nullptr, MY_HOTKEY_ID);
    g_hotkeyRegistered = false;

    ShutdownWorker();

    if (instanceMutex) CloseHandle(instanceMutex);

    LOG_INFO("VerseLink shutdown complete");
    return success ? 0 : 1;
}
