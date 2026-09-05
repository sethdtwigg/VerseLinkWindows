#include "SelfTest.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "VerseFormatter.h"
#include "VerseRetrieveInterface.h"

#include <iostream>
#include <string>
#include <vector>

namespace SelfTest {
namespace {

int g_checks = 0;
std::vector<std::string> g_failures;

void Check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        g_failures.push_back(what);
    }
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// The subset of settings that changes what gets pasted.
struct FormattingSettings {
    bool includeReferenceInReplacement;
    bool referenceOnFirstLine;
    bool includeVerseNumbers;
    bool dynamicReference;
    std::string replacementFormat;
};

FormattingSettings Capture() {
    const VerseLinkConfig config = ConfigManager::getInstance().getConfig();
    return { config.includeReferenceInReplacement, config.referenceOnFirstLine,
             config.includeVerseNumbers, config.dynamicReference, config.replacementFormat };
}

void Apply(const FormattingSettings& settings) {
    auto& config = ConfigManager::getInstance();
    config.setIncludeReferenceInReplacement(settings.includeReferenceInReplacement);
    config.setReferenceOnFirstLine(settings.referenceOnFirstLine);
    config.setIncludeVerseNumbers(settings.includeVerseNumbers);
    config.setDynamicReference(settings.dynamicReference);
    config.setReplacementFormat(settings.replacementFormat);
}

struct Combination {
    const char* name;
    FormattingSettings settings;
};

std::vector<Combination> Combinations() {
    return {
        { "defaults",
          { true,  false, false, false, "{reference} {text}" } },
        { "verseNumbers",
          { true,  false, true,  false, "{reference} {text}" } },
        // The configuration the range problem was reported under.
        { "dynamicReference+verseNumbers",
          { true,  false, true,  true,  "{reference} {text}" } },
        // Exercises includeReferenceInReplacement=false, which used to do the
        // opposite of what it says.
        { "referenceExcluded",
          { false, false, false, false, "{reference} {text}" } },
        // What the shipped config.json actually sets.
        { "referenceOnFirstLine",
          { true,  true,  true,  false, "{reference} {text}" } },
    };
}

// Independently derived expectation - deliberately not a call into
// VerseFormatter, so a bug there is caught rather than mirrored.
std::string ExpectedComposition(const FormattingSettings& settings,
                                const std::string& reference,
                                const std::string& verseText) {
    if (!settings.includeReferenceInReplacement || reference.empty()) {
        return verseText;
    }
    if (settings.referenceOnFirstLine) {
        return reference + "\n" + verseText;
    }
    return reference + " " + verseText; // the default "{reference} {text}" template
}

struct Expectation {
    const char* reference;
    const char* mustContain;      // text from the first verse of the span
    const char* mustAlsoContain;  // text from the last verse of the span
    const char* mustNotContain;   // text from just past the end of the span
};

std::vector<Expectation> Expectations() {
    return {
        // single verse
        { "John 3:16", "God so loved", "everlasting life", "condemned already" },
        // same-chapter range: verses 1..5, so verse 6 must not leak in
        { "Romans 8:1-5", "no condemnation", "things of the Spirit", "carnally minded is death" },
        // cross-chapter range: 8:28 through 9:1
        { "Romans 8:28-9:1", "work together for good", "I lie not", "great heaviness" },
        // whole chapter
        { "Psalm 23", "my shepherd", "for ever", "The earth is the LORD" },
        // explicit verse list
        { "John 3:16,17", "God so loved", "might be saved", "condemned already" },
        // chapter range: chapters 1..2, so chapter 3 must not leak in
        { "John 1-2", "In the beginning was the Word", "he knew what was in man", "man of the Pharisees" },
    };
}

void RunReferenceCases(const std::string& version) {
    for (const auto& combination : Combinations()) {
        Apply(combination.settings);
        const VerseLinkConfig config = ConfigManager::getInstance().getConfig();

        for (const auto& expectation : Expectations()) {
            const std::string label =
                std::string(expectation.reference) + " [" + combination.name + "]";

            VerseRetrieveInterface vri(expectation.reference, version);
            const bool ok = vri.GetVerseText();
            Check(ok, label + ": lookup should succeed");
            if (!ok) continue;

            Check(Contains(vri.VerseText, expectation.mustContain),
                  label + ": missing start of span (\"" + expectation.mustContain + "\")");
            Check(Contains(vri.VerseText, expectation.mustAlsoContain),
                  label + ": missing end of span (\"" + expectation.mustAlsoContain + "\")");
            Check(!Contains(vri.VerseText, expectation.mustNotContain),
                  label + ": leaked text from past the end of the span (\"" +
                  expectation.mustNotContain + "\")");

            const std::string reference =
                vri.ReferenceText.empty() ? std::string(expectation.reference) : vri.ReferenceText;
            const std::string actual =
                VerseFormatter::ComposeReplacementText(config, reference, vri.VerseText);
            const std::string expected =
                ExpectedComposition(combination.settings, reference, vri.VerseText);

            Check(actual == expected, label + ": composed replacement text differs\n" +
                                      "      expected: " + expected.substr(0, 120) + "\n" +
                                      "      actual:   " + actual.substr(0, 120));

            if (combination.settings.includeReferenceInReplacement) {
                Check(Contains(actual, reference), label + ": reference should be present");
            } else {
                Check(actual == vri.VerseText,
                      label + ": reference must be absent when includeReferenceInReplacement is false");
            }
        }
    }
}

void RunFailureCases(const std::string& version) {
    // A reference that resolves to no verses must report failure. Reporting
    // success here made the app paste a bare reference over the selection.
    const char* unresolvable[] = { "Genesis 999", "John 3:999", "Habbakuk 1:1", "just some prose" };

    for (const auto& combination : Combinations()) {
        Apply(combination.settings);
        for (const char* input : unresolvable) {
            VerseRetrieveInterface vri(input, version);
            const std::string label = std::string(input) + " [" + combination.name + "]";
            Check(!vri.GetVerseText(), label + ": must not report success");
            Check(vri.VerseText.empty(), label + ": must produce no verse text");
        }
    }
}

void RunDashCases(const std::string& version) {
    // Written as escapes so this file stays ASCII-only; a literal dash here is
    // exactly what got corrupted before.
    const struct { const char* name; const char* input; } variants[] = {
        { "ascii hyphen",        "Romans 8:1-5" },
        { "en dash",             "Romans 8:1\xE2\x80\x93""5" },
        { "em dash",             "Romans 8:1\xE2\x80\x94""5" },
        { "non-breaking hyphen", "Romans 8:1\xE2\x80\x91""5" },
        { "minus sign",          "Romans 8:1\xE2\x88\x92""5" },
        { "nbsp around dash",    "Romans 8:1\xC2\xA0-\xC2\xA0""5" },
        { "replacement char",    "Romans 8:1\xEF\xBF\xBD""5" },
    };

    Apply(Combinations().front().settings); // defaults

    std::string reference;
    std::string text;
    {
        VerseRetrieveInterface baseline("Romans 8:1-5", version);
        Check(baseline.GetVerseText(), "dash baseline: ASCII hyphen must resolve");
        reference = baseline.ReferenceText;
        text = baseline.VerseText;
    }

    for (const auto& variant : variants) {
        VerseRetrieveInterface vri(variant.input, version);
        const std::string label = std::string("dash variant: ") + variant.name;
        const bool ok = vri.GetVerseText();
        Check(ok, label + ": must resolve");
        if (!ok) continue;
        Check(vri.ReferenceText == reference,
              label + ": reference should match the ASCII form (got '" + vri.ReferenceText + "')");
        Check(vri.VerseText == text, label + ": verse text should match the ASCII form");
    }
}

std::string ResolveVersion() {
    auto& config = ConfigManager::getInstance();
    const std::string version = config.getBibleVersion();
    return version.empty() ? std::string("KJV.xml") : version;
}

void SilenceLogging() {
    // No file, no console: the self test owns stdout.
    Logger::initialize("", LogLevel::Error, false, false);
}

} // namespace

int Run() {
    SilenceLogging();

    const std::string version = ResolveVersion();
    const std::string resolved =
        Bible::FindBibleFilePath(version, ConfigManager::getInstance().getBibleDataPath());
    if (resolved.empty()) {
        std::cerr << "SELFTEST: could not locate Bible XML '" << version
                  << "'. Run from the repository root, or place Bibles/ next to the exe."
                  << std::endl;
        return 1;
    }
    std::cout << "SELFTEST: using " << resolved << std::endl;

    RunReferenceCases(version);
    RunFailureCases(version);
    RunDashCases(version);

    if (g_failures.empty()) {
        std::cout << "SELFTEST: all " << g_checks << " checks passed" << std::endl;
        return 0;
    }

    std::cout << "SELFTEST: " << (g_checks - g_failures.size()) << "/" << g_checks
              << " checks passed; " << g_failures.size() << " FAILED:" << std::endl;
    for (const auto& failure : g_failures) {
        std::cout << "  FAIL: " << failure << std::endl;
    }
    return 1;
}

int PrintVerse(const std::string& reference) {
    SilenceLogging();

    const VerseLinkConfig config = ConfigManager::getInstance().getConfig();
    VerseRetrieveInterface vri(reference, ResolveVersion());

    if (!vri.GetVerseText()) {
        std::cerr << "Could not resolve '" << reference << "': "
                  << (vri.LastError.empty() ? "no matching verses" : vri.LastError) << std::endl;
        std::cerr << vri.GetLog();
        return 1;
    }

    const std::string label = vri.ReferenceText.empty() ? reference : vri.ReferenceText;
    std::cout << VerseFormatter::ComposeReplacementText(config, label, vri.VerseText) << std::endl;
    return 0;
}

} // namespace SelfTest
