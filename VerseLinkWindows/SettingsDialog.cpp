#include "SettingsDialog.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "SystemTray.h"
#include "StringExtensions.h"
#include "VerseLinkWindows.h"
#include "Bible.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

SettingsDialog* SettingsDialog::instance = nullptr;

namespace {
    bool EnsureSettingsClassRegistered() {
        static bool registered = false;
        if (registered) return true;

        WNDCLASS wc = {};
        wc.lpfnWndProc = SettingsDialog::DialogProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"VerseLinkSettingsDialog";

        if (!RegisterClass(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        registered = true;
        return true;
    }
}

SettingsDialog::SettingsDialog(HWND parentHwnd) : hParent(parentHwnd), hwnd(nullptr) {
}

SettingsDialog::~SettingsDialog() {
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

bool SettingsDialog::Show() {
    instance = this;
    
    if (!EnsureSettingsClassRegistered()) {
        LogMessage("Failed to register dialog class");
        instance = nullptr;
        return false;
    }
    
    // Create dialog window with proper class
    hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW,
        L"VerseLinkSettingsDialog",
        L"VerseLink Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_THICKFRAME,
        100, 100, 450, 420, // Proper size
        hParent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!hwnd) {
        LogMessage("Failed to create settings dialog");
        instance = nullptr;
        return false;
    }
    
    CreateControls();
    LoadCurrentSettings();
    
    // Show dialog
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    // Modal message loop. While it runs, thread messages (no target window)
    // arrive here instead of the main loop, so they must be handled explicitly:
    // - WM_QUIT is re-posted so the main message loop can shut the app down.
    // - WM_HOTKEY would otherwise be silently dropped by DispatchMessage.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        
        if (msg.message == WM_HOTKEY && msg.wParam == MY_HOTKEY_ID) {
            QueueHotkeyTask();
            continue; // keep servicing the dialog
        }
        
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Check if window was destroyed
        if (!IsWindow(hwnd)) {
            break;
        }
    }
    
    if (instance == this) {
        instance = nullptr;
    }
    
    return true;
}

void SettingsDialog::CreateControls() {
    // Create a better settings layout with proper spacing
    const int LABEL_WIDTH = 120;
    const int FIELD_WIDTH = 280;
    const int FIELD_HEIGHT = 22;
    const int VERTICAL_SPACING = 30;
    const int HORIZONTAL_SPACING = 10;
    const int START_X = 15;
    const int START_Y = 15;
    const int WINDOW_WIDTH = 500;
    const int WINDOW_HEIGHT = 650; // Increased to accommodate verse formatting controls
    
    // Set window size properly
    SetWindowPos(hwnd, nullptr, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
    
    int currentY = START_Y;
    
    // Bible Version (drop-down of discovered Bible XML files)
    CreateWindowW(L"STATIC", L"Bible Version:", WS_VISIBLE | WS_CHILD,
        START_X, currentY, LABEL_WIDTH, FIELD_HEIGHT, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        START_X + LABEL_WIDTH + HORIZONTAL_SPACING, currentY, FIELD_WIDTH, 200, hwnd, (HMENU)ID_BIBLE_VERSION, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Debug Mode
    CreateWindowW(L"BUTTON", L"Debug Mode", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_DEBUG_MODE, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Include Reference in Replacement
    CreateWindowW(L"BUTTON", L"Include Reference in Replacement", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_INCLUDE_REFERENCE, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Prefer Direct Selection
    CreateWindowW(L"BUTTON", L"Prefer Direct Text Selection", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_PREFER_DIRECT_SELECTION, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Use Existing Clipboard
    CreateWindowW(L"BUTTON", L"Use Existing Clipboard First", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_USE_EXISTING_CLIPBOARD, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Enable Logging
    CreateWindowW(L"BUTTON", L"Enable Logging", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_ENABLE_LOGGING, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Log File Path
    CreateWindowW(L"STATIC", L"Log File Path:", WS_VISIBLE | WS_CHILD,
        START_X, currentY, LABEL_WIDTH, FIELD_HEIGHT, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        START_X + LABEL_WIDTH + HORIZONTAL_SPACING, currentY, FIELD_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_LOG_FILE_PATH, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Replacement Format
    CreateWindowW(L"STATIC", L"Verse Format:", WS_VISIBLE | WS_CHILD,
        START_X, currentY, LABEL_WIDTH, FIELD_HEIGHT, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        START_X + LABEL_WIDTH + HORIZONTAL_SPACING, currentY, FIELD_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_REPLACEMENT_FORMAT, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Icon Path
    CreateWindowW(L"STATIC", L"Icon Path:", WS_VISIBLE | WS_CHILD,
        START_X, currentY, LABEL_WIDTH, FIELD_HEIGHT, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        START_X + LABEL_WIDTH + HORIZONTAL_SPACING, currentY, FIELD_WIDTH - 60, FIELD_HEIGHT, hwnd, (HMENU)ID_ICON_PATH, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"BUTTON", L"Browse...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        START_X + LABEL_WIDTH + FIELD_WIDTH - 45, currentY, 60, FIELD_HEIGHT, hwnd, (HMENU)ID_BROWSE_ICON, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING + 10; // Extra space before verse formatting
    
    // Verse Formatting Section
    CreateWindowW(L"STATIC", L"Verse Formatting:", WS_VISIBLE | WS_CHILD,
        START_X, currentY, LABEL_WIDTH, FIELD_HEIGHT, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Include Verse Numbers
    CreateWindowW(L"BUTTON", L"Include Verse Numbers", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_INCLUDE_VERSE_NUMBERS, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // New Line Between Chapters
    CreateWindowW(L"BUTTON", L"New Line Between Chapters", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_NEWLINE_BETWEEN_CHAPTERS, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // New Line Between Books
    CreateWindowW(L"BUTTON", L"New Line Between Books", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_NEWLINE_BETWEEN_BOOKS, GetModuleHandle(nullptr), nullptr);
    currentY += VERTICAL_SPACING;
    
    // Reference on First Line
    CreateWindowW(L"BUTTON", L"Reference on First Line", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_REFERENCE_ON_FIRST_LINE, GetModuleHandle(nullptr), nullptr);

    currentY += VERTICAL_SPACING;

    // Dynamic Reference
    CreateWindowW(L"BUTTON", L"Dynamic Reference", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        START_X, currentY, FIELD_WIDTH + LABEL_WIDTH, FIELD_HEIGHT, hwnd, (HMENU)ID_DYNAMIC_REFERENCE, GetModuleHandle(nullptr), nullptr);

    currentY += VERTICAL_SPACING + 10; // Extra space before buttons
    
    // Buttons - properly positioned
    int buttonY = currentY;
    int buttonWidth = 100;
    int buttonHeight = 28;
    int buttonSpacing = 10;
    
    CreateWindowW(L"BUTTON", L"Save", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        START_X, buttonY, buttonWidth, buttonHeight, hwnd, (HMENU)ID_SAVE, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        START_X + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight, hwnd, (HMENU)ID_CANCEL, GetModuleHandle(nullptr), nullptr);
    CreateWindowW(L"BUTTON", L"Reset to Defaults", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        START_X + (buttonWidth + buttonSpacing) * 2, buttonY, buttonWidth + 40, buttonHeight, hwnd, (HMENU)ID_RESET_DEFAULTS, GetModuleHandle(nullptr), nullptr);
}

void SettingsDialog::PopulateBibleVersions(const std::string& currentVersion) {
    HWND hCombo = GetDlgItem(hwnd, ID_BIBLE_VERSION);
    if (!hCombo) return;

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

    std::string current = currentVersion;
    if (current.empty()) {
        current = ConfigManager::getInstance().getBibleVersion();
    }

    auto versions = Bible::FindAvailableBibleVersions(ConfigManager::getInstance().getBibleDataPath());
    bool hasCurrent = false;
    for (const auto& version : versions) {
        // UTF-8 -> wide for display
        std::wstring wide = StringExtensions::Utf8ToWide(version);
        LRESULT index = SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wide.c_str());
        if (!hasCurrent && _wcsicmp(wide.c_str(), StringExtensions::Utf8ToWide(current).c_str()) == 0) {
            hasCurrent = true;
            SendMessage(hCombo, CB_SETCURSEL, index, 0);
        }
    }

    if (!current.empty() && !hasCurrent) {
        // Current value is not on disk (e.g. file removed); keep it selectable.
        std::wstring wide = StringExtensions::Utf8ToWide(current);
        LPARAM index = SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wide.c_str());
        SendMessage(hCombo, CB_SETCURSEL, index, 0);
    }
}

void SettingsDialog::LoadCurrentSettings() {
    auto& config = ConfigManager::getInstance();
    
    // Store original values
    originalValues[ID_BIBLE_VERSION] = config.getBibleVersion();
    originalValues[ID_DEBUG_MODE] = config.isDebugMode() ? "true" : "false";
    originalValues[ID_INCLUDE_REFERENCE] = config.includeReferenceInReplacement() ? "true" : "false";
    originalValues[ID_PREFER_DIRECT_SELECTION] = config.preferDirectSelection() ? "true" : "false";
    originalValues[ID_USE_EXISTING_CLIPBOARD] = config.useExistingClipboard() ? "true" : "false";
    originalValues[ID_ENABLE_LOGGING] = config.isLoggingEnabled() ? "true" : "false";
    originalValues[ID_LOG_FILE_PATH] = config.getLogFilePath();
    originalValues[ID_REPLACEMENT_FORMAT] = config.getReplacementFormat();
    originalValues[ID_ICON_PATH] = config.getIconPath();
    // Verse formatting options
    originalValues[ID_INCLUDE_VERSE_NUMBERS] = config.includeVerseNumbers() ? "true" : "false";
    originalValues[ID_NEWLINE_BETWEEN_CHAPTERS] = config.newLineBetweenChapters() ? "true" : "false";
    originalValues[ID_NEWLINE_BETWEEN_BOOKS] = config.newLineBetweenBooks() ? "true" : "false";
    originalValues[ID_REFERENCE_ON_FIRST_LINE] = config.referenceOnFirstLine() ? "true" : "false";
    originalValues[ID_DYNAMIC_REFERENCE] = config.dynamicReference() ? "true" : "false";
    
    // Set control values
    PopulateBibleVersions(config.getBibleVersion());
    SetControlChecked(ID_DEBUG_MODE, config.isDebugMode());
    SetControlChecked(ID_INCLUDE_REFERENCE, config.includeReferenceInReplacement());
    SetControlChecked(ID_PREFER_DIRECT_SELECTION, config.preferDirectSelection());
    SetControlChecked(ID_USE_EXISTING_CLIPBOARD, config.useExistingClipboard());
    SetControlChecked(ID_ENABLE_LOGGING, config.isLoggingEnabled());
    SetControlText(ID_LOG_FILE_PATH, config.getLogFilePath());
    SetControlText(ID_REPLACEMENT_FORMAT, config.getReplacementFormat());
    SetControlText(ID_ICON_PATH, config.getIconPath());
    // Set verse formatting controls
    SetControlChecked(ID_INCLUDE_VERSE_NUMBERS, config.includeVerseNumbers());
    SetControlChecked(ID_NEWLINE_BETWEEN_CHAPTERS, config.newLineBetweenChapters());
    SetControlChecked(ID_NEWLINE_BETWEEN_BOOKS, config.newLineBetweenBooks());
    SetControlChecked(ID_REFERENCE_ON_FIRST_LINE, config.referenceOnFirstLine());
    SetControlChecked(ID_DYNAMIC_REFERENCE, config.dynamicReference());
    
    LogMessage("Settings loaded into dialog");
}

void SettingsDialog::SaveSettings() {
    auto& config = ConfigManager::getInstance();
    
    // Begin batch update to prevent multiple callbacks
    config.beginBatchUpdate();
    
    // Update configuration
    config.setBibleVersion(GetControlText(ID_BIBLE_VERSION));
    config.setDebugMode(GetControlChecked(ID_DEBUG_MODE));
    config.setIncludeReferenceInReplacement(GetControlChecked(ID_INCLUDE_REFERENCE));
    config.setPreferDirectSelection(GetControlChecked(ID_PREFER_DIRECT_SELECTION));
    config.setUseExistingClipboard(GetControlChecked(ID_USE_EXISTING_CLIPBOARD));
    config.setLoggingEnabled(GetControlChecked(ID_ENABLE_LOGGING));
    config.setLogFilePath(GetControlText(ID_LOG_FILE_PATH));
    config.setReplacementFormat(GetControlText(ID_REPLACEMENT_FORMAT));
    config.setIconPath(GetControlText(ID_ICON_PATH));
    // Save verse formatting options
    config.setIncludeVerseNumbers(GetControlChecked(ID_INCLUDE_VERSE_NUMBERS));
    config.setNewLineBetweenChapters(GetControlChecked(ID_NEWLINE_BETWEEN_CHAPTERS));
    config.setNewLineBetweenBooks(GetControlChecked(ID_NEWLINE_BETWEEN_BOOKS));
    config.setReferenceOnFirstLine(GetControlChecked(ID_REFERENCE_ON_FIRST_LINE));
    config.setDynamicReference(GetControlChecked(ID_DYNAMIC_REFERENCE));
    
    // End batch update - this will trigger the callback once
    config.endBatchUpdate();
    
    // Save to file
    if (config.save()) {
        MessageBoxW(hwnd, L"Settings saved successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
        LogMessage("Settings saved successfully");
        
        // Update system tray icon if icon path changed
        std::string newIconPath = GetControlText(ID_ICON_PATH);
        if (newIconPath != originalValues[ID_ICON_PATH]) {
            // Find the main application window and update system tray
            HWND mainWnd = FindWindow(L"VerseLinkHiddenWindow", nullptr);
            if (mainWnd) {
                // Get system tray instance from main window
                SystemTray* systemTray = reinterpret_cast<SystemTray*>(GetWindowLongPtr(mainWnd, GWLP_USERDATA));
                if (systemTray) {
                    systemTray->SetCustomIcon(newIconPath);
                    LogMessage("Updated system tray icon from settings: " + newIconPath);
                } else {
                    LogMessage("Could not get system tray instance from main window");
                }
            } else {
                LogMessage("Could not find main application window");
            }
        }
    } else {
        MessageBoxW(hwnd, L"Failed to save settings!", L"Error", MB_OK | MB_ICONERROR);
        LogMessage("Failed to save settings");
    }
}

void SettingsDialog::ResetToDefaults() {
    if (MessageBoxW(hwnd, L"Reset all settings to default values?", L"Confirm Reset", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        
        // Reset controls to default values
        SetControlText(ID_BIBLE_VERSION, "KJV.xml");
        SetControlChecked(ID_DEBUG_MODE, false);
        SetControlChecked(ID_INCLUDE_REFERENCE, true);
        SetControlChecked(ID_PREFER_DIRECT_SELECTION, true);
        SetControlChecked(ID_USE_EXISTING_CLIPBOARD, true);
        SetControlChecked(ID_ENABLE_LOGGING, true);
        SetControlText(ID_LOG_FILE_PATH, "verselink.log");
        SetControlText(ID_REPLACEMENT_FORMAT, "{reference} {text}");
        SetControlText(ID_ICON_PATH, ""); // empty = use default icon
        // Reset verse formatting controls to defaults
        SetControlChecked(ID_INCLUDE_VERSE_NUMBERS, false);
        SetControlChecked(ID_NEWLINE_BETWEEN_CHAPTERS, false);
        SetControlChecked(ID_NEWLINE_BETWEEN_BOOKS, false);
        SetControlChecked(ID_REFERENCE_ON_FIRST_LINE, false);
        SetControlChecked(ID_DYNAMIC_REFERENCE, false);
        
        LogMessage("Settings reset to defaults");
    }
}

std::string SettingsDialog::GetControlText(int controlId) {
    HWND hControl = GetDlgItem(hwnd, controlId);
    if (hControl) {
        int length = GetWindowTextLengthW(hControl);
        if (length > 0) {
            std::wstring wide(length + 1, L'\0');
            int copied = GetWindowTextW(hControl, &wide[0], length + 1);
            wide.resize(copied > 0 ? copied : 0);
            // WideToUtf8 of the resized string contains no embedded terminator,
            // so comparisons against stored values behave correctly.
            return StringExtensions::WideToUtf8(wide);
        }
    }
    return "";
}

void SettingsDialog::SetControlText(int controlId, const std::string& text) {
    HWND hControl = GetDlgItem(hwnd, controlId);
    if (!hControl) return;

    // Combo boxes need item selection rather than window text
    wchar_t className[32] = {};
    GetClassNameW(hControl, className, 32);
    if (_wcsicmp(className, L"combobox") == 0) {
        std::wstring wide = StringExtensions::Utf8ToWide(text);
        LRESULT index = SendMessageW(hControl, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)wide.c_str());
        if (index == CB_ERR) {
            index = SendMessageW(hControl, CB_ADDSTRING, 0, (LPARAM)wide.c_str());
        }
        if (index != CB_ERR) {
            SendMessage(hControl, CB_SETCURSEL, index, 0);
        }
        return;
    }

    SetWindowTextW(hControl, StringExtensions::Utf8ToWide(text).c_str());
}

bool SettingsDialog::GetControlChecked(int controlId) {
    HWND hControl = GetDlgItem(hwnd, controlId);
    if (hControl) {
        return SendMessage(hControl, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    return false;
}

void SettingsDialog::SetControlChecked(int controlId, bool checked) {
    HWND hControl = GetDlgItem(hwnd, controlId);
    if (hControl) {
        SendMessage(hControl, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void SettingsDialog::LogMessage(const std::string& message) {
    LOG_INFO("SettingsDialog: " + message);
}

void SettingsDialog::BrowseForIcon() {
    // Open file dialog for icon selection
    OPENFILENAME ofn = {};
    wchar_t szFile[260] = L"";
    
    // Initialize OPENFILENAME structure
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Icon Files\0*.ico\0All Files\0*.*\0\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Select Icon File";
    
    // Show the file dialog
    if (GetOpenFileName(&ofn)) {
        // Convert wide string to narrow string using Windows API
        int size = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string selectedPath(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &selectedPath[0], size, nullptr, nullptr);
            
            // Set the selected path to the icon path field
            SetControlText(ID_ICON_PATH, selectedPath);
            LogMessage("Selected icon file: " + selectedPath);
        }
    }
}

LRESULT CALLBACK SettingsDialog::DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hDlg, &ps);
                
                // Fill background with system color
                RECT rect;
                GetClientRect(hDlg, &rect);
                HBRUSH hBrush = GetSysColorBrush(COLOR_WINDOW);
                FillRect(hdc, &rect, hBrush);
                
                EndPaint(hDlg, &ps);
                return 0;
            }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_SAVE:
                    if (instance) {
                        instance->SaveSettings();
                    }
                    DestroyWindow(hDlg);
                    return 0;
                    
                case ID_CANCEL:
                    DestroyWindow(hDlg);
                    return 0;
                    
                case ID_RESET_DEFAULTS:
                    if (instance) {
                        instance->ResetToDefaults();
                    }
                    return 0;
                    
                case ID_BROWSE_ICON:
                    if (instance) {
                        instance->BrowseForIcon();
                    }
                    return 0;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hDlg);
            return 0;
            
        case WM_DESTROY:
            // Don't call PostQuitMessage - it closes the entire app.
            // Clear the stale instance pointer so it is never dereferenced
            // after the window is gone.
            if (instance && instance->hwnd == hDlg) {
                instance->hwnd = nullptr;
            }
            return 0;
    }
    
    return DefWindowProc(hDlg, message, wParam, lParam);
}
