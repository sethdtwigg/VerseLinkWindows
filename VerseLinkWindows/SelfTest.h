#pragma once
#ifndef SelfTest_H
#define SelfTest_H

#include <string>

// Headless entry points for the exe, used by CI and for reproducing a user
// report without a hotkey, a window or a clipboard.
//
// Where tests/test_harness.cpp covers the logic units, this covers the wiring
// VerseLinkTask actually uses: a real reference goes through
// VerseRetrieveInterface and VerseFormatter::ComposeReplacementText, and the
// exact string that would have been pasted is asserted.
//
// Neither entry point writes to disk or registers a settings callback, so
// running them cannot disturb a user's config.json.
namespace SelfTest {
    // Runs every case. Returns 0 when all pass, 1 otherwise.
    int Run();

    // Prints the replacement text a reference would produce. Returns 0 on
    // success, 1 when the reference could not be resolved.
    int PrintVerse(const std::string& reference);
}

#endif
