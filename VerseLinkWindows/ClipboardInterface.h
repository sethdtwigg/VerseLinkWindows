#pragma once
#ifndef ClipboardInterface_H
#define ClipboardInterface_H

#include "StringExtensions.h"
#include <windows.h>
#include <uiautomation.h>
#include <memory>

class ClipboardInterface
{
    HWND m_target_window = NULL;
    std::string m_last_error;
    
    // UI Automation helpers
    std::string GetSelectedTextUsingUIAutomation();
    std::string GetSelectedTextFromEditControl();
    std::string GetSelectedTextUsingClipboard();
    bool SendKeys(const std::vector<WORD>& keys);
    std::string LogError(const std::string& message);

public:
    std::string Log;
    std::string GetLastError() const { return m_last_error; }
    std::string GetLog() const { return Log; }
    
    ClipboardInterface();
    std::string GetSelectedText();
    bool ReplaceSelectedText(const std::string& newText);
    ~ClipboardInterface();
};

#endif