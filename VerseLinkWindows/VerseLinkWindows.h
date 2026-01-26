#pragma once

#include <iostream>
#include <iterator>
#include <thread>
#include <string>
#include <chrono>
#include <windows.h>

#include "ClipboardInterface.h"
#include "VerseRetrieveInterface.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "SystemTray.h"

const int MY_HOTKEY_ID = 1;
extern std::string BV;
extern bool Logging;
extern bool Debugging;

// Function declarations
void Log(const std::string& message);
void VerseLinkTask();
bool RunVerseLink(HWND hwnd, SystemTray* systemTray);
bool GetConfiguration();
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);