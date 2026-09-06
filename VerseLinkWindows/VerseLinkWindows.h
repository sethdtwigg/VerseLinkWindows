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
// Scratch id used to test whether a combination is available before giving up
// the one currently registered. Never left registered.
const int PROBE_HOTKEY_ID = 2;

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

// Renders a hotkey as something a person can read, e.g. "Ctrl+Alt+L", so UI
// text reflects the configured combination instead of a hardcoded one.
std::string DescribeHotkey(int modifiers, int virtualKey);

// Turns a configured logFilePath into the path actually being written: absolute
// paths as-is, relative ones resolved beside config.json. Anything that reads
// the log must go through this, or it will read a different file than the one
// the logger is writing.
std::string ResolveLogPath(const std::string& configuredPath);

// Function declarations
void VerseLinkTask();
bool RunVerseLink(HWND hwnd, SystemTray* systemTray);
bool GetConfiguration();
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);