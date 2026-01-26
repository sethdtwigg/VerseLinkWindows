#pragma once
#ifndef SystemTray_H
#define SystemTray_H

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <memory>

#define WM_TRAYICON (WM_USER + 1)

class SystemTray {
private:
    NOTIFYICONDATA nid;
    HMENU hPopupMenu;
    HWND hwnd;
    bool isVisible;
    HICON hCustomIcon;
    
    // Menu item IDs
    enum MenuItems {
        ID_SETTINGS = 1001,
        ID_VIEW_LOG = 1002,
        ID_ABOUT = 1003,
        ID_QUIT = 1004
    };
    
    void CreateTrayMenu();
    void ShowSettingsDialog();
    void ShowAboutDialog();
    void ShowLogDialog();
    void LogMessage(const std::string& message);
    HICON LoadCustomIcon(const std::string& iconPath);
    
public:
    SystemTray(HWND parentHwnd);
    ~SystemTray();
    
    bool Initialize();
    void Show();
    void Hide();
    void UpdateTooltip(const std::string& tooltip);
    void SetCustomIcon(const std::string& iconPath);
    void HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Cleanup();
};

#endif
