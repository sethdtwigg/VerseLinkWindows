#pragma once
#ifndef SettingsDialog_H
#define SettingsDialog_H

#include <windows.h>
#include <string>
#include <map>

class SettingsDialog {
private:
    HWND hwnd;
    HWND hParent;
    
    // Control IDs
    enum Controls {
        ID_BIBLE_VERSION = 1001,
        ID_DEBUG_MODE = 1002,
        ID_INCLUDE_REFERENCE = 1003,
        ID_PREFER_DIRECT_SELECTION = 1004,
        ID_USE_EXISTING_CLIPBOARD = 1005,
        ID_ENABLE_LOGGING = 1006,
        ID_LOG_FILE_PATH = 1007,
        ID_REPLACEMENT_FORMAT = 1008,
        ID_HOTKEY_MODIFIERS = 1009,
        ID_HOTKEY_KEY = 1010,
        ID_ICON_PATH = 1011,
        ID_BROWSE_ICON = 1012,
        ID_SAVE = 1013,
        ID_CANCEL = 1014,
        ID_RESET_DEFAULTS = 1015,
        // Verse formatting options
        ID_INCLUDE_VERSE_NUMBERS = 1016,
        ID_NEWLINE_BETWEEN_CHAPTERS = 1017,
        ID_NEWLINE_BETWEEN_BOOKS = 1018,
        ID_REFERENCE_ON_FIRST_LINE = 1019,
        ID_DYNAMIC_REFERENCE = 1020
    };
    
    // Original values for comparison
    std::map<int, std::string> originalValues;
    
    void CreateControls();
    void LoadCurrentSettings();
    void SaveSettings();
    void ResetToDefaults();
    void BrowseForIcon();
    std::string GetControlText(int controlId);
    void SetControlText(int controlId, const std::string& text);
    bool GetControlChecked(int controlId);
    void SetControlChecked(int controlId, bool checked);
    void LogMessage(const std::string& message);
    
public:
    SettingsDialog(HWND parentHwnd);
    ~SettingsDialog();
    
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    bool Show();
    
private:
    static SettingsDialog* instance;
};

#endif
