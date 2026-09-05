// VerseLink test harness
// Links the pure-logic modules (Bible, VerseRetrieveInterface, ConfigManager,
// Logger, tinyxml2) and exercises reference parsing, verse retrieval against
// the real KJV.xml, and config load/save round-trips. No GUI dependencies.
//
// Run from the repository root so "Bibles/KJV.xml" resolves.

#include "VerseRetrieveInterface.h"
#include "Bible.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "StringExtensions.h"
#include "TaskQueue.h"
#include "VerseFormatter.h"
#include "tinyxml2.h"

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

static int g_checks = 0;
static std::vector<std::string> g_failures;
static std::string g_section;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            g_failures.push_back("[" + g_section + "] " + msg +              \
                                 "  (" __FILE__ ":" + std::to_string(__LINE__) + ")"); \
        }                                                                    \
    } while (0)

static void BeginSection(const std::string& name) {
    g_section = name;
    std::cout << "--- " << name << " ---" << std::endl;
}

// Parses input and returns the single expected reference (empty vector on failure).
static std::vector<Bible::BibleReference> Parse(const std::string& input) {
    VerseRetrieveInterface vri(input, "unused.xml");
    return vri.ParsedReferences();
}

struct RetrievalResult {
    bool ok = false;
    std::string verseText;
    std::string refText;
    std::string log;
};

static RetrievalResult Retrieve(const std::string& input, const std::string& versionPath) {
    RetrievalResult r;
    VerseRetrieveInterface vri(input, versionPath);
    r.ok = vri.GetVerseText();
    r.verseText = vri.VerseText;
    r.refText = vri.ReferenceText;
    r.log = vri.GetLog();
    return r;
}

static void TestParsing() {
    BeginSection("Parsing");

    auto one = [](const std::string& in) {
        auto v = Parse(in);
        return v.size() == 1 ? v[0] : Bible::BibleReference();
    };

    // Single verses
    {
        auto br = one("John 3:16");
        CHECK(!br.BookName.empty(), "John 3:16 should parse");
        CHECK(br.BookName == "John", "John 3:16 book, got '" + br.BookName + "'");
        CHECK(br.ChapterNumber == "3", "John 3:16 chapter, got '" + br.ChapterNumber + "'");
        CHECK(br.VerseNumber == "16", "John 3:16 verse, got '" + br.VerseNumber + "'");
        CHECK(br.Type == Bible::ReferenceType::SINGLE_VERSE, "John 3:16 type");
    }
    {
        auto br = one("jn 3:16");
        CHECK(br.BookName == "John", "jn 3:16 abbreviation resolves to John, got '" + br.BookName + "'");
    }
    {
        auto br = one("1 samuel 2:3");
        CHECK(br.BookName == "1 Samuel", "1 samuel 2:3 book, got '" + br.BookName + "'");
    }
    {
        auto br = one("Bogusbook 3:16");
        CHECK(br.BookName.empty(), "Unknown book must not parse");
    }

    // Ranges
    {
        auto br = one("Romans 8:1-5");
        CHECK(br.BookName == "Romans" && br.VerseNumber == "1" && br.EndVerseNumber == "5",
              "Romans 8:1-5 fields, got book='" + br.BookName + "' v='" + br.VerseNumber +
              "' endV='" + br.EndVerseNumber + "'");
        CHECK(br.Type == Bible::ReferenceType::VERSE_RANGE && br.IsVerseRange(), "Romans 8:1-5 type");
    }
    {
        auto br = one("Romans 8:28-9:1");
        CHECK(br.BookName == "Romans" && br.ChapterNumber == "8" && br.VerseNumber == "28" &&
              br.EndChapterNumber == "9" && br.EndVerseNumber == "1",
              "Romans 8:28-9:1 cross-chapter fields");
        CHECK(br.IsVerseRange(), "Romans 8:28-9:1 type VERSE_RANGE");
    }
    {
        auto br = one("Genesis 1");
        CHECK(br.BookName == "Genesis" && br.ChapterNumber == "1" &&
              br.Type == Bible::ReferenceType::CHAPTER_ONLY, "Genesis 1 chapter-only");
    }
    {
        auto br = one("John 1-2");
        CHECK(br.BookName == "John" && br.ChapterNumber == "1" && br.EndChapterNumber == "2" &&
              br.IsChapterRange(), "John 1-2 chapter range");
    }
    {
        auto br = one("Genesis - Exodus");
        CHECK(br.BookName == "Genesis" && br.EndBookName == "Exodus" && br.IsBookRange(),
              "Genesis - Exodus book range");
    }
    {
        auto br = one("Jonah 1 - Micah 1");
        CHECK(br.BookName == "Jonah" && br.EndBookName == "Micah" &&
              br.ChapterNumber == "1" && br.EndChapterNumber == "1" && br.IsBookRange(),
              "Jonah 1 - Micah 1 book range with chapters");
    }

    // Multiple verses - KNOWN BROKEN pre-fix (comma split makes this unreachable)
    {
        auto br = one("John 3:16,18,20");
        CHECK(br.Type == Bible::ReferenceType::MULTIPLE_VERSES,
              "John 3:16,18,20 must parse as MULTIPLE_VERSES (got type=" +
              std::to_string((int)br.Type) + ")");
        CHECK(br.VerseNumber == "16,18,20", "multi-verse list preserved, got '" + br.VerseNumber + "'");
    }
}

static void TestRetrieval() {
    BeginSection("Retrieval (KJV.xml)");
    const std::string kjv = "Bibles/KJV.xml";

    // Single verse
    {
        auto r = Retrieve("John 3:16", kjv);
        CHECK(r.ok, "John 3:16 retrieval succeeds; lastError/log tail: " +
                    r.log.substr(r.log.size() > 300 ? r.log.size() - 300 : 0));
        CHECK(r.verseText.find("God so loved") != std::string::npos, "John 3:16 text content");
        CHECK(r.refText == "John 3:16", "John 3:16 reference text, got '" + r.refText + "'");
    }
    {
        auto r = Retrieve("gen 1:1", kjv);
        CHECK(r.ok && r.verseText.find("In the beginning God created") != std::string::npos,
              "gen 1:1 retrieval via abbreviation");
    }
    // Verse range
    {
        auto r = Retrieve("Psalm 23:1-2", kjv);
        CHECK(r.ok && r.verseText.find("green pastures") != std::string::npos,
              "Psalm 23:1-2 range retrieval");
    }
    // Chapter only
    {
        auto r = Retrieve("Psalm 23", kjv);
        CHECK(r.ok && r.verseText.find("still waters") != std::string::npos &&
                      r.verseText.find("dwell in the house of the LORD for ever") != std::string::npos,
              "Psalm 23 full chapter (first+last verse present)");
    }
    {
        auto r = Retrieve("Genesis 1", kjv);
        CHECK(r.ok && r.verseText.find("heaven and the earth") != std::string::npos,
              "Genesis 1 chapter-only retrieval");
    }
    // Multiple verses - KNOWN BROKEN pre-fix
    {
        auto r = Retrieve("John 3:16,17", kjv);
        CHECK(r.ok, "John 3:16,17 multi-verse retrieval succeeds");
        if (r.ok) {
            CHECK(r.verseText.find("God so loved") != std::string::npos &&
                  r.verseText.find("might be saved") != std::string::npos,
                  "John 3:16,17 contains both verses");
        }
    }
    // Cross-chapter range
    {
        auto r = Retrieve("Romans 8:28-9:1", kjv);
        CHECK(r.ok && r.verseText.find("work together") != std::string::npos &&
                      r.verseText.find("I lie not") != std::string::npos,
              "Romans 8:28-9:1 cross-chapter retrieval spans both chapters");
    }
    // Book range (small books only to keep runtime sane)
    {
        auto r = Retrieve("Jonah - Micah", kjv);
        CHECK(r.ok && !r.verseText.empty(), "Jonah - Micah whole-book range non-empty");
    }
}

static void TestUnicodeHelpers() {
    BeginSection("Unicode helpers (StringExtensions)");
    using namespace StringExtensions;

    // ASCII round-trip
    CHECK(WideToUtf8(Utf8ToWide("John 3:16")) == "John 3:16", "ASCII round-trip");

    // Multi-byte content: curly quotes, em dash, Greek, and an emoji (surrogate pair).
    // The old byte-wise wstring(begin,end) widening corrupted all of these.
    std::string utf8 = "\xE2\x80\x9Cgrace\xE2\x80\x9D \xE2\x80\x94 \xCE\xA7\xCE\xAC\xCF\x81\xCE\xB9\xCF\x82 \xF0\x9F\x8E\xB5";
    std::string roundTrip = WideToUtf8(Utf8ToWide(utf8));
    CHECK(roundTrip == utf8, "UTF-8 round-trip preserves multibyte content");
    if (roundTrip != utf8) {
        std::cout << "  expected bytes: ";
        for (unsigned char c : utf8) printf("%02X ", c);
        std::cout << std::endl << "  actual bytes:   ";
        for (unsigned char c : roundTrip) printf("%02X ", c);
        std::cout << std::endl;
    }

    CHECK(!Utf8ToWide(utf8).empty(), "Utf8ToWide produces non-empty wide string");
    CHECK(WideToUtf8(L"") == "" && Utf8ToWide("").empty(), "empty strings handled");
}

static void TestConfigConcurrency() {
    BeginSection("Config concurrency stress");

    const std::string cfgPath = "tests/test_config.json";
    ConfigManager::initialize(cfgPath);
    auto& cm = ConfigManager::getInstance();

    const int durationMs = 1500;
    std::atomic<bool> stop(false);
    std::atomic<long long> mutations(0), reads(0);

    std::thread writer([&] {
        bool toggle = false;
        while (!stop) {
            cm.setIncludeVerseNumbers(toggle);
            cm.setDynamicReference(toggle);
            cm.setReplacementFormat("{reference} {text}");
            toggle = !toggle;
            ++mutations;
        }
    });

    std::thread reader([&] {
        while (!stop) {
            // Snapshot copy must never observe a torn string
            VerseLinkConfig snapshot = cm.getConfig();
            size_t len = snapshot.replacementFormat.size();
            if (snapshot.bibleVersion.empty()) {
                g_failures.push_back("[" + g_section + "] getConfig() observed empty bibleVersion");
            }
            volatile size_t sink = cm.getReplacementFormat().size() + len;
            (void)sink;
            cm.includeVerseNumbers();
            ++reads;
        }
    });

    std::thread saver([&] {
        while (!stop) {
            cm.save();
            Sleep(20);
        }
    });

    Sleep(durationMs);
    stop = true;
    writer.join();
    reader.join();
    saver.join();

    std::cout << "  (" << mutations << " mutations, " << reads << " read cycles)" << std::endl;
    CHECK(mutations > 10 && reads > 10, "stress test actually exercised the config");
    ConfigManager::initialize(cfgPath); // restore known state for any later checks
}

static void TestConfigRoundTrip() {
    BeginSection("Config round-trip");

    const std::string cfgPath = "tests/test_config.json";
    const char* json =
        "{\n"
        "  \"bibleVersion\": \"KJV.xml\",\n"
        "  \"bibleDataPath\": \"X:/bibles\",\n"
        "  \"hotkeyModifiers\": 6,\n"
        "  \"hotkeyVirtualKey\": 74,\n"
        "  \"enableLogging\": true,\n"
        "  \"logFilePath\": \"custom.log\",\n"
        "  \"logLevel\": 3,\n"
        "  \"debugMode\": false,\n"
        "  \"includeVerseNumbers\": true,\n"
        "  \"dynamicReference\": true,\n"
        "  \"replacementFormat\": \"{reference}: {text}\"\n"
        "}\n";
    { std::ofstream f(cfgPath); f << json; }

    ConfigManager::initialize(cfgPath);
    auto& cm = ConfigManager::getInstance();

    // Values that were never parsed pre-fix are asserted here.
    CHECK(cm.getHotkeyModifiers() == 6, "hotkeyModifiers loaded from file (got " +
          std::to_string(cm.getHotkeyModifiers()) + ")");
    CHECK(cm.getHotkeyVirtualKey() == 74, "hotkeyVirtualKey loaded from file (got " +
          std::to_string(cm.getHotkeyVirtualKey()) + ")");
    CHECK(cm.getLogFilePath() == "custom.log", "logFilePath loaded from file (got '" +
          cm.getLogFilePath() + "')");
    CHECK(cm.getBibleDataPath() == "X:/bibles", "bibleDataPath loaded from file (got '" +
          cm.getBibleDataPath() + "')");

    // Round-trip through save(), asserted against the bytes actually written.
    //
    // Re-reading the singleton proves nothing: ConfigManager::initialize()
    // reloads into the same instance and load() never resets to defaults, so
    // the previous version of these checks passed even if save() wrote nothing.
    CHECK(cm.save(), "config save succeeds");

    std::string written;
    {
        std::ifstream saved(cfgPath);
        std::stringstream buffer;
        buffer << saved.rdbuf();
        written = buffer.str();
    }
    CHECK(!written.empty(), "saved config file is not empty");

    const char* expectedPairs[] = {
        "\"hotkeyModifiers\": 6",
        "\"hotkeyVirtualKey\": 74",
        "\"logFilePath\": \"custom.log\"",
        "\"bibleDataPath\": \"X:/bibles\"",
        "\"includeVerseNumbers\": true",
        "\"dynamicReference\": true",
        "\"replacementFormat\": \"{reference}: {text}\"",
    };
    for (const char* pair : expectedPairs) {
        CHECK(written.find(pair) != std::string::npos,
              std::string("saved config should contain ") + pair);
    }

    // Prove load() actually reads the file: point the singleton at one whose
    // values all differ, and require every one of them to change.
    const std::string reloadPath = "tests/test_config_reload.json";
    {
        std::ofstream f(reloadPath);
        f << "{\n"
             "  \"bibleVersion\": \"NASB.xml\",\n"
             "  \"bibleDataPath\": \"Y:/other\",\n"
             "  \"hotkeyModifiers\": 5,\n"
             "  \"hotkeyVirtualKey\": 75,\n"
             "  \"logFilePath\": \"other.log\",\n"
             "  \"includeVerseNumbers\": false,\n"
             "  \"dynamicReference\": false,\n"
             "  \"replacementFormat\": \"{text}\"\n"
             "}\n";
    }

    ConfigManager::initialize(reloadPath);
    auto& reloaded = ConfigManager::getInstance();
    CHECK(reloaded.getHotkeyModifiers() == 5 && reloaded.getHotkeyVirtualKey() == 75,
          "load() replaces the hotkey (got " + std::to_string(reloaded.getHotkeyModifiers()) +
          "/" + std::to_string(reloaded.getHotkeyVirtualKey()) + ")");
    CHECK(reloaded.getLogFilePath() == "other.log", "load() replaces logFilePath");
    CHECK(reloaded.getBibleDataPath() == "Y:/other", "load() replaces bibleDataPath");
    CHECK(reloaded.getReplacementFormat() == "{text}", "load() replaces replacementFormat");
    CHECK(!reloaded.includeVerseNumbers() && !reloaded.dynamicReference(),
          "load() replaces the formatting flags");

    // Leave the singleton somewhere harmless for the sections that follow.
    reloaded.setBibleDataPath("");
    reloaded.setBibleVersion("KJV.xml");
}

static void TestBibleVersionDiscovery() {
    BeginSection("Bible version discovery");
    using namespace StringExtensions;

    auto versions = Bible::FindAvailableBibleVersions();
    std::string listing;
    for (const auto& v : versions) listing += v + " ";
    CHECK(versions.size() >= 3, "at least KJV/NASB/ESV discovered (got: " + listing + ")");

    bool hasKjv = false;
    for (const auto& v : versions) {
        if (_wcsicmp(Utf8ToWide(v).c_str(), L"KJV.xml") == 0) hasKjv = true;
    }
    CHECK(hasKjv, "KJV.xml present in discovery list");

    // Sorted output
    auto sorted = versions;
    std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
    });
    CHECK(versions == sorted, "discovery list is sorted");

    // Resolver still finds a discovered version by name
    std::string resolved = Bible::FindBibleFilePath("KJV.xml", "");
    CHECK(!resolved.empty(), "FindBibleFilePath resolves KJV.xml from discovery set");
}

static void TestNonReferenceInput() {
    BeginSection("Non-reference input rejected");

    // Regression: unparseable selection used to yield a whitespace-only
    // "verse" (a bare newline), making GetVerseText() report success and the
    // app replace the user's selection with itself.
    auto r = Retrieve("Continue with Phase 5", "unused.xml");
    CHECK(!r.ok, "unparseable selection must not report success");
    CHECK(r.verseText.empty(), "no verse text produced for unparseable selection");

    auto r2 = Retrieve("just some ordinary words", "unused.xml");
    CHECK(!r2.ok && r2.verseText.empty(), "plain sentence rejected cleanly");
}

static void TestLogRotation() {
    BeginSection("Log rotation");
    namespace fs = std::filesystem;

    const std::string logPath = "tests/rotation_test.log";
    for (int i = 1; i <= 3; ++i) {
        std::error_code ec;
        fs::remove(logPath + "." + std::to_string(i), ec);
    }
    std::error_code ecRemove;
    fs::remove(logPath, ecRemove);

    // Tiny limit forces frequent rollovers
    Logger::initialize(logPath, Info, /*console*/ false, /*file*/ true,
                       /*maxFileSizeBytes*/ 1200, /*maxBackupFiles*/ 2);

    const std::string filler(90, 'x');
    for (int i = 0; i < 80; ++i) {
        LOG_INFO("rotation stress line " + std::to_string(i) + " " + filler);
    }

    bool rotatedOnce = fs::exists(logPath + ".1");
    bool backupChainRespected = !fs::exists(logPath + ".3"); // maxBackupFiles = 2

    CHECK(rotatedOnce, "log rolled over to .1");
    CHECK(backupChainRespected, "backup count respected (no .3)");

    size_t activeSize = 0;
    std::error_code ecSize;
    if (fs::exists(logPath)) {
        activeSize = fs::file_size(logPath, ecSize);
    }
    CHECK(activeSize < 4000, "active log stays bounded after rotations (size=" +
          std::to_string(activeSize) + ")");

    // Restore default logging so later sections are unaffected
    Logger::initialize("verselink.log", Info, true, true);
}

// ---------------------------------------------------------------------------
// Reference normalisation: the range failure
// ---------------------------------------------------------------------------

static void TestDashVariants() {
    BeginSection("Dash and whitespace variants");

    // Written as byte escapes so this file stays ASCII-only. A literal dash here
    // is exactly what got silently re-encoded to U+FFFD and broke every range.
    const struct { const char* name; const char* input; } variants[] = {
        { "ascii hyphen",        "Romans 8:1-5" },
        { "en dash",             "Romans 8:1\xE2\x80\x93" "5" },
        { "em dash",             "Romans 8:1\xE2\x80\x94" "5" },
        { "non-breaking hyphen", "Romans 8:1\xE2\x80\x91" "5" },
        { "figure dash",         "Romans 8:1\xE2\x80\x92" "5" },
        { "horizontal bar",      "Romans 8:1\xE2\x80\x95" "5" },
        { "minus sign",          "Romans 8:1\xE2\x88\x92" "5" },
        { "nbsp around dash",    "Romans 8:1\xC2\xA0-\xC2\xA0" "5" },
        { "replacement char",    "Romans 8:1\xEF\xBF\xBD" "5" },
        { "zero-width space",    "Romans 8:1-\xE2\x80\x8B" "5" },
        { "leading BOM",         "\xEF\xBB\xBF" "Romans 8:1-5" },
    };

    for (const auto& variant : variants) {
        auto refs = Parse(variant.input);
        const std::string label = std::string("range with ") + variant.name;
        CHECK(refs.size() == 1, label + " should parse to one reference");
        if (refs.size() != 1) continue;
        CHECK(refs[0].BookName == "Romans" && refs[0].ChapterNumber == "8" &&
              refs[0].VerseNumber == "1" && refs[0].EndVerseNumber == "5",
              label + " should yield Romans 8:1-5, got " + refs[0].BookName + " " +
              refs[0].ChapterNumber + ":" + refs[0].VerseNumber + "-" + refs[0].EndVerseNumber);
    }

    // The normaliser itself, independent of parsing.
    using StringExtensions::NormalizeReferenceText;
    CHECK(NormalizeReferenceText("Romans 8:1\xE2\x80\x93" "5") == "Romans 8:1-5",
          "en dash folds to ASCII hyphen");
    CHECK(NormalizeReferenceText("John\xC2\xA0" "3:16") == "John 3:16",
          "non-breaking space folds to a plain space");
    CHECK(NormalizeReferenceText("John\xE2\x80\x8B" "3:16") == "John3:16",
          "zero-width space is dropped");
    CHECK(NormalizeReferenceText("plain ascii") == "plain ascii",
          "ASCII text passes through untouched");
    CHECK(NormalizeReferenceText("") == "", "empty input handled");
    // Invalid UTF-8 must survive rather than be mangled further.
    CHECK(NormalizeReferenceText("\xFF\xFE") == "\xFF\xFE",
          "invalid UTF-8 bytes pass through unchanged");
    // Non-punctuation multibyte content must be preserved exactly.
    const std::string greek = "\xCE\xA7\xCE\xAC\xCF\x81\xCE\xB9\xCF\x82";
    CHECK(NormalizeReferenceText(greek) == greek, "other multibyte text is preserved");
}

// ---------------------------------------------------------------------------
// Range completeness, checked against the XML rather than against hardcoded text
// ---------------------------------------------------------------------------

// Collapses every run of whitespace to one space and trims, so oracle text and
// retrieved text can be compared without depending on separator settings.
static std::string Squeeze(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool inSpace = false;
    for (char c : text) {
        const bool isSpace = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
        if (isSpace) {
            inSpace = true;
            continue;
        }
        if (inSpace && !out.empty()) out += ' ';
        inSpace = false;
        out += c;
    }
    return out;
}

// Walks the Bible XML directly to build the set of verses a span should contain.
// Deliberately independent of VerseRetrieveInterface, so this is a real oracle
// and not a restatement of the code under test.
static std::vector<std::string> OracleVerses(const std::string& biblePath,
                                             const std::string& book,
                                             int chapter, int startVerse, int endVerse) {
    std::vector<std::string> verses;
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(biblePath.c_str()) != tinyxml2::XML_SUCCESS) return verses;

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) return verses;

    for (const tinyxml2::XMLElement* b = root->FirstChildElement(); b; b = b->NextSiblingElement()) {
        const char* bookName = b->Attribute("n");
        if (!bookName || book != bookName) continue;

        for (const tinyxml2::XMLElement* c = b->FirstChildElement(); c; c = c->NextSiblingElement()) {
            const char* chapterNumber = c->Attribute("n");
            if (!chapterNumber || std::atoi(chapterNumber) != chapter) continue;

            for (const tinyxml2::XMLElement* v = c->FirstChildElement(); v; v = v->NextSiblingElement()) {
                const char* verseNumber = v->Attribute("n");
                if (!verseNumber) continue;
                const int number = std::atoi(verseNumber);
                if (number < startVerse || number > endVerse) continue;
                const char* text = v->GetText();
                if (text) verses.push_back(Squeeze(text));
            }
            return verses;
        }
        return verses;
    }
    return verses;
}

struct Span { const char* book; int chapter; int startVerse; int endVerse; };

static void CheckSpanComplete(const std::string& kjv, const std::string& reference,
                              const std::vector<Span>& spans, const std::string& note) {
    auto r = Retrieve(reference, kjv);
    CHECK(r.ok, reference + " should resolve" + note);
    if (!r.ok) return;

    const std::string actual = Squeeze(r.verseText);

    size_t expectedCount = 0;
    size_t missing = 0;
    std::string firstMissing;

    for (const auto& span : spans) {
        const auto verses = OracleVerses(kjv, span.book, span.chapter, span.startVerse, span.endVerse);
        CHECK(!verses.empty(), reference + ": oracle found no verses for " +
              std::string(span.book) + " " + std::to_string(span.chapter));
        expectedCount += verses.size();
        for (const auto& verse : verses) {
            if (verse.empty()) continue;
            if (actual.find(verse) == std::string::npos) {
                ++missing;
                if (firstMissing.empty()) firstMissing = verse.substr(0, 60);
            }
        }
    }

    CHECK(missing == 0, reference + note + ": " + std::to_string(missing) + " of " +
          std::to_string(expectedCount) + " verses missing from the result" +
          (firstMissing.empty() ? "" : "; first missing: \"" + firstMissing + "...\""));
}

static void TestRangeCompleteness() {
    BeginSection("Range completeness (every verse in the span)");
    const std::string kjv = "Bibles/KJV.xml";

    // Verified against the XML itself, under each formatting combination that
    // changes how verses are joined - including dynamicReference, which is where
    // ranges were reported as broken.
    struct Combo {
        const char* name;
        bool includeVerseNumbers;
        bool dynamicReference;
        bool newLineBetweenChapters;
    };
    const Combo combos[] = {
        { "defaults",                      false, false, false },
        { "verseNumbers",                  true,  false, false },
        { "dynamicReference+verseNumbers", true,  true,  false },
        { "dynamicReference only",         false, true,  false },
        { "newlineBetweenChapters",        true,  false, true  },
    };

    auto& config = ConfigManager::getInstance();
    const VerseLinkConfig saved = config.getConfig();

    for (const auto& combo : combos) {
        config.setIncludeVerseNumbers(combo.includeVerseNumbers);
        config.setDynamicReference(combo.dynamicReference);
        config.setNewLineBetweenChapters(combo.newLineBetweenChapters);
        const std::string note = std::string(" [") + combo.name + "]";

        CheckSpanComplete(kjv, "Romans 8:1-5",    {{"Romans", 8, 1, 5}}, note);
        CheckSpanComplete(kjv, "Psalm 23:1-6",    {{"Psalms", 23, 1, 6}}, note);
        CheckSpanComplete(kjv, "Psalm 23",        {{"Psalms", 23, 1, 999}}, note);
        CheckSpanComplete(kjv, "Genesis 1",       {{"Genesis", 1, 1, 999}}, note);
        CheckSpanComplete(kjv, "John 3:16",       {{"John", 3, 16, 16}}, note);
        CheckSpanComplete(kjv, "Romans 8:28-9:1", {{"Romans", 8, 28, 999}, {"Romans", 9, 1, 1}}, note);
        CheckSpanComplete(kjv, "John 1-2",        {{"John", 1, 1, 999}, {"John", 2, 1, 999}}, note);

        // Upper bound: the verse just past the end must not leak in.
        {
            auto r = Retrieve("Romans 8:1-5", kjv);
            const auto beyond = OracleVerses(kjv, "Romans", 8, 6, 6);
            CHECK(r.ok && beyond.size() == 1 &&
                  Squeeze(r.verseText).find(beyond[0]) == std::string::npos,
                  "Romans 8:1-5" + note + " must not include verse 6");
        }
    }

    // Restore
    config.setIncludeVerseNumbers(saved.includeVerseNumbers);
    config.setDynamicReference(saved.dynamicReference);
    config.setNewLineBetweenChapters(saved.newLineBetweenChapters);
}

// ---------------------------------------------------------------------------
// Replacement composition and the settings that drive it
// ---------------------------------------------------------------------------

static void TestReplacementComposition() {
    BeginSection("Replacement composition");

    const std::string reference = "John 3:16";
    const std::string verse = "For God so loved the world";

    VerseLinkConfig config;
    config.includeReferenceInReplacement = true;
    config.referenceOnFirstLine = false;
    config.replacementFormat = "{reference} {text}";
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) ==
          reference + " " + verse, "default template puts reference before text");

    // Regression: this used to do the opposite of what it says, because
    // GetVerseText prepended the reference when the flag was false.
    config.includeReferenceInReplacement = false;
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) == verse,
          "includeReferenceInReplacement=false yields verse text only");

    config.includeReferenceInReplacement = true;
    config.referenceOnFirstLine = true;
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) ==
          reference + "\n" + verse, "referenceOnFirstLine puts the reference on its own line");

    config.referenceOnFirstLine = false;
    config.replacementFormat = "{text} ({reference})";
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) ==
          verse + " (" + reference + ")", "template order is honoured");

    config.replacementFormat = "{reference}: {text} -- {reference}";
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) ==
          reference + ": " + verse + " -- " + reference,
          "every occurrence of a token is replaced, not just the first");

    config.replacementFormat = "no tokens here";
    CHECK(VerseFormatter::ComposeReplacementText(config, reference, verse) ==
          reference + " " + verse,
          "a template naming no token falls back instead of pasting itself");

    config.replacementFormat = "{reference} {text}";
    CHECK(VerseFormatter::ComposeReplacementText(config, "", verse) == verse,
          "an empty reference yields verse text only");
}

static void TestNoFalseSuccess() {
    BeginSection("Unresolvable references never report success");
    const std::string kjv = "Bibles/KJV.xml";

    // A reference whose book resolves but whose chapter or verse does not used
    // to come back "successful" carrying only the reference, which the app then
    // pasted over the user's selection.
    const char* unresolvable[] = {
        "Genesis 999", "John 3:999", "Romans 8:900-905", "Psalm 23:40-45", "Continue with Phase 5"
    };

    auto& config = ConfigManager::getInstance();
    const VerseLinkConfig saved = config.getConfig();

    const bool referenceFlags[] = { true, false };
    for (bool includeReference : referenceFlags) {
        for (bool firstLine : referenceFlags) {
            config.setIncludeReferenceInReplacement(includeReference);
            config.setReferenceOnFirstLine(firstLine);
            const std::string note = std::string(" [includeReference=") +
                (includeReference ? "true" : "false") + ", firstLine=" +
                (firstLine ? "true" : "false") + "]";

            for (const char* input : unresolvable) {
                auto r = Retrieve(input, kjv);
                CHECK(!r.ok, std::string(input) + note + " must not report success");
                CHECK(r.verseText.empty(), std::string(input) + note + " must produce no verse text");
            }
        }
    }

    config.setIncludeReferenceInReplacement(saved.includeReferenceInReplacement);
    config.setReferenceOnFirstLine(saved.referenceOnFirstLine);
}

// ---------------------------------------------------------------------------
// Book aliases
// ---------------------------------------------------------------------------

static void TestBookAliases() {
    BeginSection("Book aliases");

    // Every alias must resolve to a name the XML actually uses; values like
    // "1st John" or "I Corinthians" parsed fine and then found no book.
    const struct { const char* input; const char* expected; } aliases[] = {
        { "1st John 1:9",       "1 John" },
        { "2nd John 1",         "2 John" },
        { "3rd John 1",         "3 John" },
        { "I Corinthians 13:4", "1 Corinthians" },
        { "II Corinthians 5:17","2 Corinthians" },
        { "I Timothy 1:1",      "1 Timothy" },
        { "II Timothy 2:1",     "2 Timothy" },
        { "I Thessalonians 5:16","1 Thessalonians" },
        { "II Thessalonians 3:3","2 Thessalonians" },
        { "jud 1:1",            "Judges" },   // "jud" is Judges; Jude keeps jude/jd
        { "jude 1",             "Jude" },
        { "jd 1",               "Jude" },
        // Multi-word names: the book capture group used to stop at the first
        // word, so these could not parse whatever the alias table said.
        { "Song of Solomon 2:1","Song of Solomon" },
        { "Song of Songs 2:1",  "Song of Solomon" },
        { "1 Samuel 2:3",       "1 Samuel" },
        { "2 Chronicles 7:14",  "2 Chronicles" },
    };

    for (const auto& alias : aliases) {
        auto refs = Parse(alias.input);
        CHECK(refs.size() == 1, std::string(alias.input) + " should parse");
        if (refs.size() != 1) continue;
        CHECK(refs[0].BookName == alias.expected,
              std::string(alias.input) + " should resolve to " + alias.expected +
              ", got '" + refs[0].BookName + "'");
    }

    // Invariant: every alias resolves to a canonical book, so no alias can parse
    // and then fail the XML lookup.
    const auto canonical = Bible::GetBookNames();
    for (const auto& entry : Bible::BookAliases) {
        const bool known = std::find(canonical.begin(), canonical.end(), entry.second) != canonical.end();
        CHECK(known, "alias '" + entry.first + "' maps to '" + entry.second +
                     "', which is not a canonical book name");
        std::string lowered = entry.first;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        CHECK(entry.first == lowered,
              "alias key '" + entry.first + "' is unreachable: lookups are lowercased first");

        // And it must actually resolve through the public entry point.
        CHECK(Bible::NormalizeBookName(entry.first) == entry.second,
              "alias '" + entry.first + "' should normalise to '" + entry.second +
              "', got '" + Bible::NormalizeBookName(entry.first) + "'");
    }

    // Aliases must actually retrieve, not merely parse.
    auto r = Retrieve("1st John 1:9", "Bibles/KJV.xml");
    CHECK(r.ok && r.verseText.find("faithful and just") != std::string::npos,
          "1st John 1:9 retrieves real verse text");
}

// ---------------------------------------------------------------------------
// Threading
// ---------------------------------------------------------------------------

static void TestTaskQueue() {
    BeginSection("Task queue hand-off");

    // Lost-wakeup regression. The pending flag has to be set under the same
    // mutex the waiter blocks on; setting it outside let a notify slip between
    // the waiter's predicate check and its registration on the condition
    // variable. The wakeup was lost, the flag stayed set, and every later press
    // was then coalesced away - the app went permanently deaf to the hotkey.
    TaskQueue queue;
    std::atomic<int> handled(0);

    std::thread worker([&] {
        while (queue.WaitForTask()) {
            ++handled;
        }
    });

    int posted = 0;
    for (int i = 0; i < 5000; ++i) {
        if (queue.Post()) ++posted;
    }

    // Wait for the worker to drain, with a bound so a lost wakeup fails the test
    // instead of hanging CI.
    const int timeoutMs = 5000;
    int waitedMs = 0;
    while (handled < posted && waitedMs < timeoutMs) {
        Sleep(10);
        waitedMs += 10;
    }

    CHECK(handled == posted, "every queued task was handled (posted=" +
          std::to_string(posted) + ", handled=" + std::to_string(handled.load()) +
          ") - a shortfall means a wakeup was lost");
    CHECK(posted > 0, "the queue actually accepted work");

    queue.Shutdown();
    worker.join();

    CHECK(queue.IsShuttingDown(), "queue reports shutdown");
    CHECK(!queue.Post(), "Post() is refused after shutdown");
    CHECK(!queue.WaitForTask(), "WaitForTask() returns false after shutdown");

    // Coalescing: a second post while one is pending must not queue extra work.
    TaskQueue coalescing;
    CHECK(coalescing.Post(), "first post is accepted");
    CHECK(!coalescing.Post(), "second post is coalesced into the pending one");
    CHECK(coalescing.WaitForTask(), "the pending task is delivered once");
    coalescing.Shutdown();
}

static void TestLoggerConcurrency() {
    BeginSection("Logger reconfiguration under load");

    // Logger::initialize runs on the UI thread whenever settings are saved,
    // while the worker thread may be inside log() writing to the same ofstream.
    // Without a shared lock that is a data race on a live stream.
    const std::string pathA = "tests/logger_race.log";
    const std::string pathB = "tests/logger_race_b.log";
    for (const auto& path : { pathA, pathB }) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    Logger::initialize(pathA, Info, /*console*/ false, /*file*/ true);

    std::atomic<bool> stop(false);
    std::atomic<long long> writes(0), reconfigures(0);

    std::thread logger([&] {
        while (!stop) {
            LOG_INFO("concurrent logging line that is long enough to matter");
            ++writes;
        }
    });

    std::thread reconfigurer([&] {
        bool toggle = false;
        while (!stop) {
            Logger::initialize(toggle ? pathB : pathA, Info, false, true);
            toggle = !toggle;
            ++reconfigures;
        }
    });

    Sleep(750);
    stop = true;
    logger.join();
    reconfigurer.join();

    Logger::initialize(pathA, Info, false, true);
    CHECK(writes > 10 && reconfigures > 10,
          "logger race actually exercised both threads (writes=" + std::to_string(writes) +
          ", reconfigures=" + std::to_string(reconfigures) + ")");

    // Interleaved writes must still produce whole, well-formed lines.
    size_t lines = 0, malformed = 0;
    for (const auto& path : { pathA, pathB }) {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            ++lines;
            if (line.front() != '[') ++malformed;
        }
    }
    CHECK(lines > 0, "log lines were written");
    CHECK(malformed == 0, "no torn log lines (" + std::to_string(malformed) + " of " +
          std::to_string(lines) + " malformed)");

    Logger::initialize("verselink.log", Info, true, true);
}

int main() {
    TestParsing();
    TestDashVariants();
    TestBookAliases();
    TestRetrieval();
    TestRangeCompleteness();
    TestReplacementComposition();
    TestNoFalseSuccess();
    TestUnicodeHelpers();
    TestBibleVersionDiscovery();
    TestNonReferenceInput();
    TestTaskQueue();
    TestLoggerConcurrency();
    TestConfigRoundTrip();
    TestConfigConcurrency();
    TestLogRotation();

    std::cout << std::endl;
    if (g_failures.empty()) {
        std::cout << "ALL " << g_checks << " CHECKS PASSED" << std::endl;
        return 0;
    }
    std::cout << g_checks - g_failures.size() << "/" << g_checks << " checks passed; "
              << g_failures.size() << " FAILED:" << std::endl;
    for (const auto& f : g_failures) std::cout << "  FAIL: " << f << std::endl;
    return 1;
}
