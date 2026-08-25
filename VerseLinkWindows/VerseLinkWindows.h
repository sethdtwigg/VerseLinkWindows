#pragma once

#include <iostream>
#include <iterator>
#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <windows.h>

#include "ClipboardInterface.h"
#include "VerseRetrieveInterface.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "SystemTray.h"

const int MY_HOTKEY_ID = 1;

// Runtime settings shared between the main thread and the verse worker thread.
// Logging/Debugging are atomic; the Bible version string goes through
// mutex-guarded accessors because std::string cannot be read atomically.
extern std::atomic<bool> Logging;
extern std::atomic<bool> Debugging;
std::string GetBibleVersionSetting();
void SetBibleVersionSetting(const std::string& version);

// Queues a verse task on the worker thread. Safe to call from any thread with
// a running message pump (e.g., while the settings dialog's modal loop is
// intercepting messages on the main thread).
void QueueHotkeyTask();

// Function declarations
void Log(const std::string& message);
void VerseLinkTask();
bool RunVerseLink(HWND hwnd, SystemTray* systemTray);
bool GetConfiguration();
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);