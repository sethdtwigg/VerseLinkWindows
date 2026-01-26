#include "SystemTray.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "SettingsDialog.h"
#include <fstream>
#include <sstream>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

SystemTray::SystemTray(HWND parentHwnd) : hwnd(parentHwnd), isVisible(false), hCustomIcon(nullptr) {
    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    hPopupMenu = nullptr;
}

SystemTray::~SystemTray() {
    Cleanup();
}

bool SystemTray::Initialize() {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Create popup menu
    CreateTrayMenu();
    
    // Setup NOTIFYICONDATA
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hCustomIcon ? hCustomIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(WCHAR), L"VerseLink");
    
    return true;
}

void SystemTray::CreateTrayMenu() {
    // Use ::CreatePopupMenu to avoid any macro conflicts
    hPopupMenu = ::CreatePopupMenu();
    if (!hPopupMenu) {
        LogMessage("Failed to create popup menu");
        return;
    }
    
    // Add menu items
    if (!AppendMenuW(hPopupMenu, MF_STRING, ID_SETTINGS, L"Settings")) {
        LogMessage("Failed to add Settings menu item");
    }
    if (!AppendMenuW(hPopupMenu, MF_STRING, ID_VIEW_LOG, L"View Log")) {
        LogMessage("Failed to add View Log menu item");
    }
    AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, nullptr);
    if (!AppendMenuW(hPopupMenu, MF_STRING, ID_ABOUT, L"About")) {
        LogMessage("Failed to add About menu item");
    }
    AppendMenuW(hPopupMenu, MF_SEPARATOR, 0, nullptr);
    if (!AppendMenuW(hPopupMenu, MF_STRING, ID_QUIT, L"Quit")) {
        LogMessage("Failed to add Quit menu item");
    }
    
    LogMessage("System tray menu created successfully");
}

void SystemTray::Show() {
    if (!isVisible && Shell_NotifyIcon(NIM_ADD, &nid)) {
        isVisible = true;
        LogMessage("System tray icon shown");
    }
}

void SystemTray::Hide() {
    if (isVisible && Shell_NotifyIcon(NIM_DELETE, &nid)) {
        isVisible = false;
        LogMessage("System tray icon hidden");
    }
}

void SystemTray::UpdateTooltip(const std::string& tooltip) {
    if (tooltip.length() < sizeof(nid.szTip)/sizeof(WCHAR)) {
        std::wstring wTooltip(tooltip.begin(), tooltip.end());
        wcscpy_s(nid.szTip, sizeof(nid.szTip)/sizeof(WCHAR), wTooltip.c_str());
        if (isVisible) {
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
    }
}

void SystemTray::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TRAYICON) {
        switch (LOWORD(lParam)) {
            case WM_RBUTTONDOWN:
            case WM_CONTEXTMENU:
                {
                    // Show popup menu at cursor position
                    POINT pt;
                    GetCursorPos(&pt);
                    SetForegroundWindow(hwnd);
                    
                    UINT command = TrackPopupMenu(hPopupMenu, 
                        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                        pt.x, pt.y, 0, hwnd, nullptr);
                    
                    PostMessage(hwnd, WM_NULL, 0, 0);
                    
                    // Handle menu selection
                    switch (command) {
                        case ID_SETTINGS:
                            ShowSettingsDialog();
                            break;
                        case ID_VIEW_LOG:
                            ShowLogDialog();
                            break;
                        case ID_ABOUT:
                            ShowAboutDialog();
                            break;
                        case ID_QUIT:
                            LogMessage("Quit requested from system tray");
                            PostMessage(hwnd, WM_CLOSE, 0, 0); // Close main window
                            PostQuitMessage(0); // Ensure message loop exits
                            break;
                    }
                }
                break;
                
            case WM_LBUTTONDBLCLK:
                // Double-click could show settings or perform main action
                ShowSettingsDialog();
                break;
        }
    }
}

void SystemTray::ShowSettingsDialog() {
    LogMessage("Opening settings dialog");
    
    try {
        SettingsDialog settingsDialog(hwnd);
        settingsDialog.Show();
        LogMessage("Settings dialog closed");
    } catch (const std::exception& e) {
        LogMessage("Error opening settings dialog: " + std::string(e.what()));
        MessageBoxW(hwnd, 
            L"Error opening settings dialog",
            L"VerseLink Settings", 
            MB_OK | MB_ICONERROR);
    }
}

void SystemTray::ShowAboutDialog() {
    std::wstring aboutText = 
        L"VerseLink v1.0\n\n"
        L"A Bible verse lookup and insertion tool.\n\n"
        L"Features:\n"
        L"• Hotkey-activated verse lookup (Ctrl+Alt+L)\n"
        L"• Support for single verses, ranges, chapters, and books\n"
        L"• Smart text selection with clipboard preservation\n"
        L"• Configurable Bible versions and formatting\n\n"
        L"Created with C++ and Win32 API";
    
    MessageBox(hwnd, aboutText.c_str(), L"About VerseLink", MB_OK | MB_ICONINFORMATION);
    LogMessage("About dialog displayed");
}

void SystemTray::ShowLogDialog() {
    LogMessage("Opening log viewer");
    
    // Read log file content
    std::string logContent;
    auto& config = ConfigManager::getInstance();
    std::string logPath = config.getLogFilePath();
    
    std::ifstream logFile(logPath);
    if (logFile.is_open()) {
        std::stringstream buffer;
        buffer << logFile.rdbuf();
        logContent = buffer.str();
        logFile.close();
        
        // Limit content size for display
        if (logContent.length() > 10000) {
            logContent = logContent.substr(0, 10000) + "\n\n... (truncated for display)";
        }
    } else {
        logContent = "Log file not found or could not be opened:\n" + logPath;
    }
    
    std::wstring displayText = L"VerseLink Log Contents:\n\n";
    displayText += std::wstring(logContent.begin(), logContent.end());
    
    // Create a simple scrollable text display
    MessageBox(hwnd, displayText.c_str(), L"VerseLink Log", MB_OK | MB_ICONINFORMATION);
    LogMessage("Log dialog displayed");
}

void SystemTray::LogMessage(const std::string& message) {
    LOG_INFO("SystemTray: " + message);
}

void SystemTray::Cleanup() {
    if (isVisible) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        isVisible = false;
    }
    
    if (hPopupMenu) {
        DestroyMenu(hPopupMenu);
        hPopupMenu = nullptr;
    }
    
    if (hCustomIcon) {
        DestroyIcon(hCustomIcon);
        hCustomIcon = nullptr;
    }
}

HICON SystemTray::LoadCustomIcon(const std::string& iconPath) {
    if (iconPath.empty()) {
        return nullptr;
    }
    
    // Convert to wide string
    std::wstring wIconPath(iconPath.begin(), iconPath.end());
    
    // Try multiple methods to load the icon
    
    // Method 1: Load as icon file (best for .ico files)
    HICON hIcon = (HICON)LoadImage(
        nullptr,
        wIconPath.c_str(),
        IMAGE_ICON,
        0, 0, // Use actual size from file
        LR_LOADFROMFILE | LR_DEFAULTCOLOR | LR_SHARED
    );
    
    if (!hIcon) {
        // Method 2: Try ExtractIcon (works for .ico and .exe)
        hIcon = ExtractIcon(GetModuleHandle(nullptr), wIconPath.c_str(), 0);
    }
    
    if (!hIcon) {
        // Method 3: Try LoadIcon with different sizes
        hIcon = (HICON)LoadImage(
            nullptr,
            wIconPath.c_str(),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_LOADFROMFILE | LR_DEFAULTCOLOR | LR_SHARED
        );
    }
    
    if (!hIcon) {
        LogMessage("Failed to load custom icon from file: " + iconPath);
        LogMessage("Tried LoadImage, ExtractIcon, and LoadIcon with system metrics");
    } else {
        LogMessage("Successfully loaded custom icon: " + iconPath);
    }
    
    return hIcon;
}

void SystemTray::SetCustomIcon(const std::string& iconPath) {
    // Clean up existing custom icon
    if (hCustomIcon) {
        DestroyIcon(hCustomIcon);
        hCustomIcon = nullptr;
    }
    
    // Load new custom icon
    hCustomIcon = LoadCustomIcon(iconPath);
    
    // Update system tray icon if visible and icon loaded successfully
    if (isVisible && hCustomIcon) {
        nid.hIcon = hCustomIcon;
        if (Shell_NotifyIcon(NIM_MODIFY, &nid)) {
            LogMessage("Updated system tray icon successfully");
        } else {
            LogMessage("Failed to update system tray icon");
        }
    } else if (isVisible && !hCustomIcon) {
        // Fall back to default icon if custom failed
        nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        if (Shell_NotifyIcon(NIM_MODIFY, &nid)) {
            LogMessage("Reverted to default system tray icon");
        }
    }
}
