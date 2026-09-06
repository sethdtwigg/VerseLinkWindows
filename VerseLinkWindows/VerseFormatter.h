#pragma once
#ifndef VerseFormatter_H
#define VerseFormatter_H

#include <string>
#include "ConfigManager.h"

namespace VerseFormatter {
    // Builds the text that replaces the user's selection, from the reference and
    // the verse text a lookup produced.
    //
    // Lives apart from VerseLinkTask so the composition can be asserted directly
    // in tests and by the exe's --selftest mode; VerseLinkTask calls straight
    // through to it, so the tested code is the shipped code.
    //
    // Precedence, highest first:
    //   1. includeReferenceInReplacement == false -> verse text only
    //   2. referenceOnFirstLine           == true -> "<reference>\n<verse text>"
    //   3. otherwise the replacementFormat template, with every occurrence of
    //      {reference} and {text} substituted
    //
    // An empty reference degrades to verse text only, and a template naming
    // neither token falls back to "<reference> <verse text>" rather than
    // emitting the literal template.
    std::string ComposeReplacementText(const VerseLinkConfig& config,
                                       const std::string& reference,
                                       const std::string& verseText);
}

#endif
