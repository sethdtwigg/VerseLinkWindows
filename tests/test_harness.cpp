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

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
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

    // Round-trip through save()
    CHECK(cm.save(), "config save succeeds");
    ConfigManager::initialize(cfgPath); // reloads into same singleton
    auto& cm2 = ConfigManager::getInstance();
    CHECK(cm2.getHotkeyModifiers() == 6 && cm2.getHotkeyVirtualKey() == 74,
          "hotkey survives save/reload");
    CHECK(cm2.includeVerseNumbers() && cm2.dynamicReference(), "format flags survive save/reload");
    CHECK(cm2.getReplacementFormat() == "{reference}: {text}", "replacementFormat survives save/reload");
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

int main() {
    TestParsing();
    TestRetrieval();
    TestUnicodeHelpers();
    TestBibleVersionDiscovery();
    TestNonReferenceInput();
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
