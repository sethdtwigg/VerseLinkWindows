#include "ClipboardInterface.h"
#include "ConfigManager.h"
#include <comdef.h>
#include <vector>
#include <algorithm>

// RAII wrapper for COM initialization
struct COMInitializer {
    COMInitializer() { CoInitialize(NULL); }
    ~COMInitializer() { CoUninitialize(); }
};

ClipboardInterface::ClipboardInterface()
{
    static COMInitializer comInit;
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
            _bstr_t name(elementName, false);
            Log += "UI Automation: Focused element name: " + std::string(name) + "\n";
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
                            _bstr_t text(bstrText, false);
                            result = (const char*)text;
                            if (!result.empty()) {
                                Log += "UI Automation: Successfully retrieved text: '" + result + "'\n";
                            }
                        }
                        
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
                                    _bstr_t text(bstrText, false);
                                    result = (const char*)text;
                                    if (!result.empty()) {
                                        Log += "UI Automation: Successfully retrieved text from document range: '" + result + "'\n";
                                    }
                                }
                                
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
    auto& config = ConfigManager::getInstance();
    
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
    
    // For debugging purposes, always force copy to test the functionality
    // In production, this would check if we should use existing clipboard content first
    bool forceCopy = true; // Always force copy for testing
    Log += "Clipboard: Force copy mode enabled for testing\n";
    
    if (!forceCopy && config.useExistingClipboard()) {
        // First, check if there's already text on clipboard that might be the selection
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            std::string existingClipboard;
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    existingClipboard = pszText;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            
            // If there's already text on clipboard, check if it looks like a valid Bible reference
            // This avoids unnecessary Ctrl+C operations for valid references
            if (!existingClipboard.empty()) {
                // More comprehensive check for Bible reference pattern
                bool looksLikeValidReference = false;
                
                // Check for common Bible reference patterns
                // Pattern 1: Book Chapter:Verse (e.g., "John 3:16", "Genesis 1:1-3")
                bool hasColon = existingClipboard.find(':') != std::string::npos;
                // Pattern 2: Book Chapter (e.g., "John 3", "Genesis 1")
                bool hasSpaceAndNumber = false;
                size_t spacePos = existingClipboard.find(' ');
                if (spacePos != std::string::npos && spacePos < existingClipboard.length() - 1) {
                    hasSpaceAndNumber = isdigit(existingClipboard[spacePos + 1]);
                }
                // Pattern 3: Just a number (too simple, likely not a reference)
                bool justNumber = true;
                for (char c : existingClipboard) {
                    if (!isdigit(c) && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                        justNumber = false;
                        break;
                    }
                }
                
                looksLikeValidReference = (hasColon || hasSpaceAndNumber) && !justNumber;
                
                if (looksLikeValidReference) {
                    Log += "Clipboard: Using existing clipboard content (valid reference pattern)\n";
                    return existingClipboard;
                } else {
                    Log += "Clipboard: Existing content doesn't look like valid reference, forcing copy\n";
                }
            }
        }
    } else {
        Log += "Clipboard: Force copy enabled, always copying selection\n";
    }
    
    // Always send Ctrl+C to get the actual selection
    Log += "Clipboard: Sending Ctrl+C to copy selection\n";
    
    // Save current clipboard content
    std::string oldClipboard;
    if (OpenClipboard(NULL)) {
        HANDLE hOldData = GetClipboardData(CF_TEXT);
        if (hOldData) {
            char* pszOldText = static_cast<char*>(GlobalLock(hOldData));
            if (pszOldText) {
                oldClipboard = pszOldText;
                Log += "Clipboard: Saved old clipboard: '" + oldClipboard + "'\n";
                GlobalUnlock(hOldData);
            }
        }
        CloseClipboard();
    }
    
    // Send Ctrl+C to copy selected text
    std::vector<WORD> keys = {VK_CONTROL, 'C'};
    if (!SendKeys(keys)) {
        LogError("Failed to send Ctrl+C");
        return "";
    }
    
    Sleep(300); // Increased wait for copy operation in modern applications
    
    // Get new clipboard content
    if (!OpenClipboard(NULL)) {
        LogError("Failed to open clipboard after copy");
        return "";
    }
    
    HANDLE hNewData = GetClipboardData(CF_TEXT);
    std::string selectedText;
    if (hNewData) {
        char* pszNewText = static_cast<char*>(GlobalLock(hNewData));
        if (pszNewText) {
            selectedText = pszNewText;
            Log += "Clipboard: Retrieved new content: '" + selectedText + "'\n";
            GlobalUnlock(pszNewText);
        }
    }
    CloseClipboard();
    
    // Restore original clipboard if no new content
    if (selectedText.empty() && !oldClipboard.empty()) {
        Log += "Clipboard: No new content, restoring original clipboard\n";
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, oldClipboard.size() + 1);
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    strcpy_s(pszText, oldClipboard.size() + 1, oldClipboard.c_str());
                    GlobalUnlock(hData);
                    SetClipboardData(CF_TEXT, hData);
                }
            }
            CloseClipboard();
        }
    }
    
    if (!selectedText.empty()) {
        Log += "Clipboard: Successfully retrieved selected text\n";
    } else {
        LogError("Clipboard: No text found after copy operation");
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
    
    // Save current clipboard
    Log += "ReplaceSelectedText: Saving current clipboard\n";
    if (!OpenClipboard(NULL)) {
        LogError("Failed to open clipboard for replacement");
        return false;
    }
    
    HANDLE hOldData = GetClipboardData(CF_TEXT);
    std::string oldClipboard;
    if (hOldData) {
        char* pszOldText = static_cast<char*>(GlobalLock(hOldData));
        if (pszOldText) {
            oldClipboard = pszOldText;
            Log += "ReplaceSelectedText: Saved old clipboard: '" + oldClipboard + "'\n";
            GlobalUnlock(hOldData);
        }
    }
    CloseClipboard();
    
    // Put new text on clipboard
    Log += "ReplaceSelectedText: Setting new text on clipboard\n";
    if (!OpenClipboard(NULL)) {
        LogError("Failed to open clipboard to set new text");
        return false;
    }
    
    EmptyClipboard();
    
    // Try Unicode text first (better for modern applications)
    std::wstring wNewText(newText.begin(), newText.end());
    HGLOBAL hUnicodeData = GlobalAlloc(GMEM_MOVEABLE, (wNewText.size() + 1) * sizeof(wchar_t));
    if (hUnicodeData) {
        wchar_t* pszUnicodeText = static_cast<wchar_t*>(GlobalLock(hUnicodeData));
        if (pszUnicodeText) {
            wcscpy_s(pszUnicodeText, wNewText.size() + 1, wNewText.c_str());
            GlobalUnlock(hUnicodeData);
            SetClipboardData(CF_UNICODETEXT, hUnicodeData);
            Log += "ReplaceSelectedText: Set Unicode text on clipboard\n";
        }
    }
    
    // Also set ANSI text as fallback
    HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, newText.size() + 1);
    if (hData) {
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText) {
            strcpy_s(pszText, newText.size() + 1, newText.c_str());
            GlobalUnlock(hData);
            SetClipboardData(CF_TEXT, hData);
            Log += "ReplaceSelectedText: Set ANSI text on clipboard\n";
        }
    }
    
    CloseClipboard();
    
    Log += "ReplaceSelectedText: New text placed on clipboard\n";
    
    // Verify clipboard content
    Sleep(50);
    if (OpenClipboard(NULL)) {
        HANDLE hVerifyData = GetClipboardData(CF_TEXT);
        if (hVerifyData) {
            char* pszVerifyText = static_cast<char*>(GlobalLock(hVerifyData));
            if (pszVerifyText) {
                std::string verifyText = pszVerifyText;
                Log += "ReplaceSelectedText: Verified clipboard content: '" + verifyText + "'\n";
                GlobalUnlock(hVerifyData);
            }
        }
        CloseClipboard();
    }
    
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
    
    // Restore original clipboard after a longer delay to ensure paste completes
    Log += "ReplaceSelectedText: Waiting for paste to complete\n";
    Sleep(2000); // Increased delay for modern applications
    
    // Verify the paste actually happened by checking if clipboard still has our text
    bool pasteSuccessful = false;
    if (OpenClipboard(NULL)) {
        HANDLE hVerifyData = GetClipboardData(CF_TEXT);
        if (hVerifyData) {
            char* pszVerifyText = static_cast<char*>(GlobalLock(hVerifyData));
            if (pszVerifyText) {
                std::string verifyText = pszVerifyText;
                // If clipboard still has our text, paste likely didn't happen yet
                if (verifyText == newText) {
                    Log += "ReplaceSelectedText: Clipboard still has new text, paste may not have completed\n";
                    // Wait longer and check again
                    Sleep(2000);
                    pasteSuccessful = true;
                } else {
                    Log += "ReplaceSelectedText: Clipboard content changed, paste likely successful\n";
                    pasteSuccessful = true;
                }
                GlobalUnlock(hVerifyData);
            }
        }
        CloseClipboard();
    }
    
    if (!oldClipboard.empty()) {
        Log += "ReplaceSelectedText: Restoring original clipboard\n";
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            HGLOBAL hOldData = GlobalAlloc(GMEM_MOVEABLE, oldClipboard.size() + 1);
            if (hOldData) {
                char* pszOldText = static_cast<char*>(GlobalLock(hOldData));
                if (pszOldText) {
                    strcpy_s(pszOldText, oldClipboard.size() + 1, oldClipboard.c_str());
                    GlobalUnlock(hOldData);
                    SetClipboardData(CF_TEXT, hOldData);
                }
            }
            CloseClipboard();
        }
    }
    
    if (!pasteSuccessful) {
        Log += "ReplaceSelectedText: Warning - paste may not have completed successfully\n";
    }
    
    Log += "ReplaceSelectedText: Replacement process completed\n";
    return success;
}

ClipboardInterface::~ClipboardInterface()
{
    m_target_window = NULL;
}


