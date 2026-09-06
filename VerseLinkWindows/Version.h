#pragma once
#ifndef Version_H
#define Version_H

// Single source of version truth. Consumed by:
//   - VerseLink.rc      -> the exe's VERSIONINFO resource (Explorer, Add/Remove
//                          Programs, and what the installer reports)
//   - SystemTray.cpp    -> the About dialog
//   - packaging/*.ps1   -> the installer's AppVersion, parsed from this file
//
// Bump VERSELINK_VERSION_STRING and the three numbers together. The installer
// compares versions to decide whether an install is an upgrade, so a release
// must never reuse a version that has already shipped.
#define VERSELINK_VERSION_MAJOR 1
#define VERSELINK_VERSION_MINOR 1
#define VERSELINK_VERSION_PATCH 1
#define VERSELINK_VERSION_STRING "1.1.1"

// Wide form for Win32 UI text, derived rather than duplicated so the two can
// never drift apart.
#define VERSELINK_WIDEN_(x) L ## x
#define VERSELINK_WIDEN(x) VERSELINK_WIDEN_(x)
#define VERSELINK_VERSION_WIDE VERSELINK_WIDEN(VERSELINK_VERSION_STRING)

// Name of the single-instance mutex the app holds while running. The installer
// declares the same name in its AppMutex setting so it can detect a running
// copy and ask the user to close it, rather than failing on a locked exe part
// way through an upgrade. Changing this breaks that handshake with every
// installer already in the wild, so leave it alone.
#define VERSELINK_INSTANCE_MUTEX L"VerseLinkWindows.SingleInstance"

// Resource identifiers
#define IDI_VERSELINK 101

#endif
