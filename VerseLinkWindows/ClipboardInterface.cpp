#include "ClipboardInterface.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <comdef.h>
#include <vector>
#include <algorithm>

// Per-thread RAII wrapper for COM initialization. CoInitialize must be called
// on every thread that uses COM (UI Automation), and a successful call -
// including S_FALSE - must be balanced by CoUninitialize. A previous
// function-local static only initialized the first worker thread's apartment.
struct COMInitializer {
    bool initialized = false;
    COMInitializer() {
        HRESULT hr = CoInitialize(NULL);
        // S_OK and S_FALSE both require a balancing CoUninitialize.
        initialized = SUCCEEDED(hr);
    }
    ~COMInitializer() {
        if (initialized) {
            CoUninitialize();
        }
    }
};

namespace {
    void RestoreClipboardText(const std::string& utf8Text) {
        if (!ConfigManager::getInstance().useExistingClipboard()) {
            return; // user opted out of touching clipboard state on failure paths
        }
        if (!OpenClipboard(NULL)) {
            LOG_ERROR("Failed to open clipboard while restoring previous content");
            return;
        }
        EmptyClipboard();
        bool ok = false;
        std::wstring wide = StringExtensions::Utf8ToWide(utf8Text);
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (hGlobal) {
            if (wchar_t* psz = static_cast<wchar_t*>(GlobalLock(hGlobal))) {
                wcscpy_s(psz, wide.size() + 1, wide.c_str());
                GlobalUnlock(hGlobal);
                if (SetClipboardData(CF_UNICODETEXT, hGlobal) != NULL) {
                    ok = true;
                }
            }
            if (!ok) {
                GlobalFree(hGlobal);
            }
        }
        CloseClipboard();
        if (!ok) {
            LOG_ERROR("Failed to restore previous clipboard content");
        }
    }

    std::string ReadClipboardTextUtf8() {
        if (!OpenClipboard(NULL)) return std::string();
        std::string result;
        if (HANDLE hData = GetClipboardData(CF_UNICODETEXT)) {
            if (wchar_t* psz = static_cast<wchar_t*>(GlobalLock(hData))) {
                result = StringExtensions::WideToUtf8(psz, static_cast<int>(wcslen(psz)));
                GlobalUnlock(hData);
            }
        } else if (HANDLE hData = GetClipboardData(CF_TEXT)) {
            if (char* psz = static_cast<char*>(GlobalLock(hData))) {
                result = StringExtensions::AnsiToUtf8(psz);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
        return result;
    }

    bool WriteClipboardText(const std::string& utf8Text) {
        // Windows editors expect CRLF line endings; internal text uses bare LF.
        std::string normalized;
        normalized.reserve(utf8Text.size() + 8);
        for (size_t i = 0; i < utf8Text.size(); ++i) {
            char c = utf8Text[i];
            if (c == '\n' && (i == 0 || utf8Text[i - 1] != '\r')) {
                normalized += "\r\n";
            } else {
                normalized += c;
            }
        }

        if (!OpenClipboard(NULL)) return false;
        EmptyClipboard();
        bool ok = false;
        std::wstring wide = StringExtensions::Utf8ToWide(normalized);
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (hGlobal) {
            if (wchar_t* psz = static_cast<wchar_t*>(GlobalLock(hGlobal))) {
                wcscpy_s(psz, wide.size() + 1, wide.c_str());
                GlobalUnlock(hGlobal);
                // On success ownership transfers to the system; do not free.
                if (SetClipboardData(CF_UNICODETEXT, hGlobal) != NULL) {
                    ok = true;
                }
            }
            if (!ok) {
                GlobalFree(hGlobal);
            }
        }
        CloseClipboard();
        return ok;
    }
}

ClipboardInterface::ClipboardInterface()
{
    m_com.reset(new COMInitializer());
    Log = "";
    m_target_window = GetForegroundWindow();
    
    if (!m_target_window) {
        LogError("Failed to get foreground window");
    } else {
        Log += "Successfully got target window\n";
    }
}

std::string ClipboardInterface::LogError(const std::string& message) {
    m_last_error = message;
    Log += "ERROR: " + message + "\n";
    return m_last_error;
}

std::string ClipboardInterface::GetSelectedTextUsingUIAutomation() {
    IUIAutomationElement* pElement = nullptr;
    IUIAutomation* pAutomation = nullptr;
    std::string result = "";
    
    try {
        // Create UI Automation instance
        HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, NULL, 
                                    CLSCTX_INPROC_SERVER, IID_IUIAutomation, 
                                    (void**)&pAutomation);
        if (FAILED(hr) || !pAutomation) {
            LogError("Failed to create UI Automation instance");
            return "";
        }
        
        // Get element with focus
        hr = pAutomation->GetFocusedElement(&pElement);
        if (FAILED(hr) || !pElement) {
            LogError("Failed to get focused element");
            pAutomation->Release();
            return "";
        }
        
        // Get element name for debugging
        BSTR elementName = NULL;
        pElement->get_CurrentName(&elementName);
        if (elementName) {
            Log += "UI Automation: Focused element name: " +
                   StringExtensions::WideToUtf8(elementName, static_cast<int>(SysStringLen(elementName))) + "\n";
            SysFreeString(elementName);
        }
        
        // Get text pattern
        IUIAutomationTextPattern* pTextPattern = nullptr;
        hr = pElement->GetCurrentPatternAs(UIA_TextPatternId, IID_IUIAutomationTextPattern, 
                                         (void**)&pTextPattern);
        if (SUCCEEDED(hr) && pTextPattern) {
            Log += "UI Automation: Found text pattern\n";
            
            // Get selection
            IUIAutomationTextRangeArray* pSelectionArray = nullptr;
            hr = pTextPattern->GetSelection(&pSelectionArray);
            
            if (SUCCEEDED(hr) && pSelectionArray) {
                int count = 0;
                pSelectionArray->get_Length(&count);
                Log += "UI Automation: Found " + std::to_string(count) + " selections\n";
                
                if (count > 0) {
                    IUIAutomationTextRange* pTextRange = nullptr;
                    pSelectionArray->GetElement(0, &pTextRange);
                    
                    if (pTextRange) {
                        BSTR bstrText = NULL;
                        hr = pTextRange->GetText(-1, &bstrText);
                        
                        if (SUCCEEDED(hr) && bstrText) {
                            result = StringExtensions::WideToUtf8(bstrText, static_cast<int>(SysStringLen(bstrText)));
                            if (!result.empty()) {
                                Log += "UI Automation: Successfully retrieved text: '" + result + "'\n";
                            }
                        }
                        if (bstrText) SysFreeString(bstrText);
                        
                        pTextRange->Release();
                    }
                }
                pSelectionArray->Release();
            } else {
                Log += "UI Automation: No selection array found\n";
            }
            pTextPattern->Release();
        } else {
            Log += "UI Automation: No text pattern found\n";
        }
        
        // If no selection found, try to get the entire text and check if user has selected something
        if (result.empty()) {
            Log += "UI Automation: Trying document range method\n";
            // Try to get the text pattern again and get the entire text
            IUIAutomationTextPattern* pTextPattern = nullptr;
            hr = pElement->GetCurrentPatternAs(UIA_TextPatternId, IID_IUIAutomationTextPattern, 
                                             (void**)&pTextPattern);
            if (SUCCEEDED(hr) && pTextPattern) {
                IUIAutomationTextRange* pTextRange = nullptr;
                hr = pTextPattern->get_DocumentRange(&pTextRange);
                
                if (SUCCEEDED(hr) && pTextRange) {
                    Log += "UI Automation: Got document range\n";
                    // Get selection from document range
                    IUIAutomationTextRangeArray* pSelectionArray = nullptr;
                    hr = pTextPattern->GetSelection(&pSelectionArray);
                    
                    if (SUCCEEDED(hr) && pSelectionArray) {
                        int count = 0;
                        pSelectionArray->get_Length(&count);
                        Log += "UI Automation: Document range found " + std::to_string(count) + " selections\n";
                        
                        if (count > 0) {
                            IUIAutomationTextRange* pSelectedRange = nullptr;
                            pSelectionArray->GetElement(0, &pSelectedRange);
                            
                            if (pSelectedRange) {
                                BSTR bstrText = NULL;
                                hr = pSelectedRange->GetText(-1, &bstrText);
                                
                                if (SUCCEEDED(hr) && bstrText) {
                                    result = StringExtensions::WideToUtf8(bstrText, static_cast<int>(SysStringLen(bstrText)));
                                    if (!result.empty()) {
                                        Log += "UI Automation: Successfully retrieved text from document range: '" + result + "'\n";
                                    }
                                }
                                if (bstrText) SysFreeString(bstrText);
                                
                                pSelectedRange->Release();
                            }
                        }
                        pSelectionArray->Release();
                    } else {
                        Log += "UI Automation: No selection in document range\n";
                    }
                    pTextRange->Release();
                } else {
                    Log += "UI Automation: Failed to get document range\n";
                }
                pTextPattern->Release();
            }
        }
        
        if (result.empty()) {
            LogError("UI Automation: No text selection found");
        }
        
    } catch (const std::exception& e) {
        LogError(std::string("UI Automation exception: ") + e.what());
    }
    
    if (pElement) pElement->Release();
    if (pAutomation) pAutomation->Release();
    
    return result;
}

std::string ClipboardInterface::GetSelectedTextFromEditControl() {
    if (!m_target_window) {
        LogError("No target window available");
        return "";
    }
    
    // Try to get selection from standard edit control
    DWORD start, end;
    LRESULT selResult = SendMessage(m_target_window, EM_GETSEL, 
                                   reinterpret_cast<WPARAM>(&start), 
                                   reinterpret_cast<LPARAM>(&end));
    
    if (selResult == -1) {
        LogError("Not a standard edit control");
        return "";
    }
    
    int length = end - start;
    if (length <= 0) {
        LogError("No text selected in edit control");
        return "";
    }
    
    // Get the full text first
    int textLength = GetWindowTextLengthA(m_target_window);
    if (textLength == 0) {
        LogError("Edit control has no text");
        return "";
    }
    
    std::vector<char> buffer(textLength + 1);
    GetWindowTextA(m_target_window, buffer.data(), textLength + 1);
    
    std::string fullText(buffer.data());
    if (start < fullText.length() && end <= fullText.length()) {
        std::string selectedText = fullText.substr(start, length);
        Log += "Edit Control: Successfully retrieved selected text\n";
        return selectedText;
    }
    
    LogError("Invalid selection range in edit control");
    return "";
}

bool ClipboardInterface::SendKeys(const std::vector<WORD>& keys) {
    Log += "SendKeys: Starting with " + std::to_string(keys.size()) + " keys\n";
    
    std::vector<INPUT> inputs;
    
    // Key down events
    for (WORD key : keys) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        inputs.push_back(input);
        Log += "SendKeys: Added key down: " + std::to_string(key) + "\n";
    }
    
    // Key up events (reverse order)
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = *it;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(input);
        Log += "SendKeys: Added key up: " + std::to_string(*it) + "\n";
    }
    
    Log += "SendKeys: Sending " + std::to_string(inputs.size()) + " input events\n";
    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    
    if (sent != inputs.size()) {
        Log += "SendKeys: Failed to send all inputs. Sent: " + std::to_string(sent) + ", Expected: " + std::to_string(inputs.size()) + "\n";
        LogError("Failed to send all key inputs");
        return false;
    }
    
    Log += "SendKeys: Successfully sent all " + std::to_string(sent) + " inputs\n";
    Sleep(100); // Brief pause for input processing
    return true;
}

std::string ClipboardInterface::GetSelectedTextUsingClipboard() {
    // Get current foreground window to ensure we're targeting the right application
    HWND currentWindow = GetForegroundWindow();
    if (!currentWindow) {
        LogError("No foreground window available for text selection");
        return "";
    }
    
    Log += "Clipboard: Target window found: " + std::to_string(reinterpret_cast<uintptr_t>(currentWindow)) + "\n";
    
    // Bring the target window to the foreground and ensure it has focus
    SetForegroundWindow(currentWindow);
    Sleep(100); // Small delay to ensure focus is established
    
    // Save current clipboard content (Unicode preserving)
    std::string oldClipboard;
    oldClipboard = ReadClipboardTextUtf8();
    if (!oldClipboard.empty()) {
        Log += "Clipboard: Saved previous clipboard content\n";
    }
    
    // Always send Ctrl+C: copying is the only reliable way to capture the
    // live selection (previously a test-only forceCopy flag skipped this and
    // made the result depend on stale clipboard content).
    Log += "Clipboard: Sending Ctrl+C to copy selection\n";
    
    // Send Ctrl+C to copy selected text
    std::vector<WORD> keys = {VK_CONTROL, 'C'};
    if (!SendKeys(keys)) {
        LogError("Failed to send Ctrl+C");
        return "";
    }
    
    Sleep(300); // Wait for copy operation in modern applications
    
    // Get new clipboard content
    std::string selectedText = ReadClipboardTextUtf8();
    if (!selectedText.empty()) {
        Log += "Clipboard: Retrieved selection (" + std::to_string(selectedText.length()) + " chars)\n";
    } else {
        LogError("Clipboard: No text found after copy operation");
    }
    
    // Restore original clipboard if no new content
    if (selectedText.empty() && !oldClipboard.empty()) {
        Log += "Clipboard: No new content, restoring original clipboard\n";
        RestoreClipboardText(oldClipboard);
    }
    
    return selectedText;
}

std::string ClipboardInterface::GetSelectedText() {
    Log = "";
    
    auto& config = ConfigManager::getInstance();
    
    // Try direct selection methods first if enabled
    if (config.preferDirectSelection()) {
        // Try UI Automation first (most reliable for modern apps)
        std::string result = GetSelectedTextUsingUIAutomation();
        if (!result.empty()) {
            return result;
        }
        
        // Try standard edit control
        result = GetSelectedTextFromEditControl();
        if (!result.empty()) {
            return result;
        }
    }
    
    // Always try clipboard method as fallback
    std::string result = GetSelectedTextUsingClipboard();
    return result;
}

bool ClipboardInterface::ReplaceSelectedText(const std::string& newText) {
    if (newText.empty()) {
        LogError("Cannot replace with empty text");
        return false;
    }
    
    Log += "ReplaceSelectedText: Starting replacement with text: '" + newText + "'\n";
    
    // Get current foreground window to ensure we're targeting the right application
    HWND currentWindow = GetForegroundWindow();
    if (!currentWindow) {
        LogError("No foreground window available for text replacement");
        return false;
    }
    
    Log += "ReplaceSelectedText: Target window found: " + std::to_string(reinterpret_cast<uintptr_t>(currentWindow)) + "\n";
    
    // Bring the target window to the foreground and ensure it has focus
    Log += "ReplaceSelectedText: Setting foreground window\n";
    SetForegroundWindow(currentWindow);
    Sleep(100); // Small delay to ensure focus is established
    
    // Verify focus was established
    HWND focusedWindow = GetForegroundWindow();
    if (focusedWindow != currentWindow) {
        Log += "ReplaceSelectedText: Warning - focus changed, retrying\n";
        SetForegroundWindow(currentWindow);
        Sleep(100);
        focusedWindow = GetForegroundWindow();
        if (focusedWindow != currentWindow) {
            LogError("ReplaceSelectedText: Failed to establish focus on target window");
            return false;
        }
    }
    
    // Save current clipboard (Unicode preserving)
    Log += "ReplaceSelectedText: Saving current clipboard\n";
    std::string oldClipboard = ReadClipboardTextUtf8();
    if (!oldClipboard.empty()) {
        Log += "ReplaceSelectedText: Saved previous clipboard content\n";
    }
    
    // Put new text on clipboard
    Log += "ReplaceSelectedText: Setting new text on clipboard\n";
    if (!WriteClipboardText(newText)) {
        // Never send Ctrl+V with a clipboard we did not successfully fill:
        // that would paste nothing and simply delete the user's selection.
        LogError("Failed to set replacement text on clipboard; aborting before paste");
        if (!oldClipboard.empty()) {
            RestoreClipboardText(oldClipboard);
        }
        return false;
    }
    
    Log += "ReplaceSelectedText: New text placed on clipboard\n";
    
    // Ensure the target window is still focused before sending paste
    Sleep(50);
    HWND checkWindow = GetForegroundWindow();
    if (checkWindow != currentWindow) {
        Log += "ReplaceSelectedText: Focus lost before paste, restoring\n";
        SetForegroundWindow(currentWindow);
        Sleep(50);
    }
    
    // Send Ctrl+V to paste
    Log += "ReplaceSelectedText: Sending Ctrl+V to paste\n";
    
    // Try multiple methods for paste with better timing
    bool success = false;
    
    // Method 1: Standard Ctrl+V with longer delay
    Sleep(200); // Ensure focus is fully established
    success = SendKeys({VK_CONTROL, 'V'});
    if (success) {
        Log += "ReplaceSelectedText: Successfully sent Ctrl+V (method 1)\n";
        Sleep(500); // Wait for paste to process
    } else {
        Log += "ReplaceSelectedText: Failed to send Ctrl+V (method 1), trying alternative\n";
        
        // Method 2: Try with left control and different timing
        Sleep(200);
        success = SendKeys({VK_LCONTROL, 'V'});
        if (success) {
            Log += "ReplaceSelectedText: Successfully sent Ctrl+V (method 2 - left control)\n";
            Sleep(500);
        } else {
            Log += "ReplaceSelectedText: Failed to send Ctrl+V (method 2), trying method 3\n";
            
            // Method 3: Try with right control and different timing
            Sleep(200);
            success = SendKeys({VK_RCONTROL, 'V'});
            if (success) {
                Log += "ReplaceSelectedText: Successfully sent Ctrl+V (method 3 - right control)\n";
                Sleep(500);
            } else {
                LogError("Failed to send paste command with all methods");
            }
        }
    }
    
    // Additional verification - try to simulate a manual paste
    if (!success) {
        Log += "ReplaceSelectedText: All methods failed, trying alternative approach\n";
        
        // Try sending individual key events with more delay
        Sleep(100);
        
        // Send Ctrl down
        INPUT ctrlDown = {};
        ctrlDown.type = INPUT_KEYBOARD;
        ctrlDown.ki.wVk = VK_CONTROL;
        SendInput(1, &ctrlDown, sizeof(INPUT));
        
        Sleep(50);
        
        // Send V down
        INPUT vDown = {};
        vDown.type = INPUT_KEYBOARD;
        vDown.ki.wVk = 'V';
        SendInput(1, &vDown, sizeof(INPUT));
        
        Sleep(50);
        
        // Send V up
        INPUT vUp = {};
        vUp.type = INPUT_KEYBOARD;
        vUp.ki.wVk = 'V';
        vUp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &vUp, sizeof(INPUT));
        
        Sleep(50);
        
        // Send Ctrl up
        INPUT ctrlUp = {};
        ctrlUp.type = INPUT_KEYBOARD;
        ctrlUp.ki.wVk = VK_CONTROL;
        ctrlUp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ctrlUp, sizeof(INPUT));
        
        Log += "ReplaceSelectedText: Sent manual Ctrl+V sequence\n";
        Sleep(500);
        success = true;
    }
    
    // Give the paste time to complete before restoring the original clipboard.
    Log += "ReplaceSelectedText: Waiting for paste to complete\n";
    Sleep(1500);
    
    if (!oldClipboard.empty()) {
        Log += "ReplaceSelectedText: Restoring original clipboard\n";
        RestoreClipboardText(oldClipboard);
    }
    
    if (!success) {
        Log += "ReplaceSelectedText: Paste keys could not be delivered\n";
    }
    
    Log += "ReplaceSelectedText: Replacement process completed\n";
    return success;
}

ClipboardInterface::~ClipboardInterface()
{
    m_target_window = NULL;
}


