#include "Bible.h"
#include <algorithm>
#include <cctype>

namespace Bible {
    // Use static const to reduce stack usage and ensure single initialization
    const std::map<std::string, std::string> BookAliases = {
        // Old Testament
        {"genesis", "Genesis"}, {"gen", "Genesis"}, {"ge", "Genesis"}, {"gn", "Genesis"},
        {"exodus", "Exodus"}, {"exo", "Exodus"}, {"ex", "Exodus"},
        {"leviticus", "Leviticus"}, {"lev", "Leviticus"}, {"le", "Leviticus"}, {"lv", "Leviticus"},
        {"numbers", "Numbers"}, {"num", "Numbers"}, {"nu", "Numbers"}, {"nm", "Numbers"},
        {"deuteronomy", "Deuteronomy"}, {"deut", "Deuteronomy"}, {"de", "Deuteronomy"}, {"dt", "Deuteronomy"},
        {"joshua", "Joshua"}, {"josh", "Joshua"}, {"jos", "Joshua"}, {"jsh", "Joshua"},
        {"judges", "Judges"}, {"judg", "Judges"}, {"jud", "Judges"}, {"jdg", "Judges"},
        {"ruth", "Ruth"}, {"rut", "Ruth"}, {"ru", "Ruth"},
        {"1 samuel", "1 Samuel"}, {"1sam", "1 Samuel"}, {"1 sa", "1 Samuel"}, {"1s", "1 Samuel"},
        {"2 samuel", "2 Samuel"}, {"2sam", "2 Samuel"}, {"2 sa", "2 Samuel"}, {"2s", "2 Samuel"},
        {"1 kings", "1 Kings"}, {"1kings", "1 Kings"}, {"1 ki", "1 Kings"}, {"1k", "1 Kings"},
        {"2 kings", "2 Kings"}, {"2kings", "2 Kings"}, {"2 ki", "2 Kings"}, {"2k", "2 Kings"},
        {"1 chronicles", "1 Chronicles"}, {"1chron", "1 Chronicles"}, {"1 ch", "1 Chronicles"}, {"1chr", "1 Chronicles"},
        {"2 chronicles", "2 Chronicles"}, {"2chron", "2 Chronicles"}, {"2 ch", "2 Chronicles"}, {"2chr", "2 Chronicles"},
        {"ezra", "Ezra"}, {"ezr", "Ezra"},
        {"nehemiah", "Nehemiah"}, {"neh", "Nehemiah"}, {"ne", "Nehemiah"},
        {"esther", "Esther"}, {"est", "Esther"}, {"es", "Esther"},
        {"job", "Job"}, {"jb", "Job"},
        {"psalms", "Psalms"}, {"psalm", "Psalms"}, {"ps", "Psalms"}, {"psa", "Psalms"},
        {"proverbs", "Proverbs"}, {"prov", "Proverbs"}, {"pro", "Proverbs"}, {"pr", "Proverbs"},
        {"ecclesiastes", "Ecclesiastes"}, {"eccles", "Ecclesiastes"}, {"ecc", "Ecclesiastes"}, {"ec", "Ecclesiastes"},
        {"song of solomon", "Song of Solomon"}, {"song of songs", "Song of Solomon"}, 
        {"song", "Song of Solomon"}, {"sng", "Song of Solomon"}, {"ss", "Song of Solomon"},
        {"isaiah", "Isaiah"}, {"isa", "Isaiah"}, {"is", "Isaiah"},
        {"jeremiah", "Jeremiah"}, {"jer", "Jeremiah"}, {"je", "Jeremiah"}, {"jr", "Jeremiah"},
        {"lamentations", "Lamentations"}, {"lam", "Lamentations"}, {"la", "Lamentations"},
        {"ezekiel", "Ezekiel"}, {"eze", "Ezekiel"}, {"ez", "Ezekiel"}, {"ezk", "Ezekiel"},
        {"daniel", "Daniel"}, {"dan", "Daniel"}, {"da", "Daniel"}, {"dn", "Daniel"},
        {"hosea", "Hosea"}, {"hos", "Hosea"}, {"ho", "Hosea"},
        {"joel", "Joel"}, {"joe", "Joel"}, {"jl", "Joel"},
        {"amos", "Amos"}, {"amo", "Amos"}, {"am", "Amos"},
        {"obadiah", "Obadiah"}, {"oba", "Obadiah"}, {"ob", "Obadiah"},
        {"jonah", "Jonah"}, {"jon", "Jonah"}, {"jnh", "Jonah"},
        {"micah", "Micah"}, {"mic", "Micah"}, {"mi", "Micah"},
        {"nahum", "Nahum"}, {"nah", "Nahum"}, {"na", "Nahum"}, {"nam", "Nahum"},
        {"habakkuk", "Habakkuk"}, {"hab", "Habakkuk"}, {"ha", "Habakkuk"},
        {"zephaniah", "Zephaniah"}, {"zep", "Zephaniah"}, {"ze", "Zephaniah"},
        {"haggai", "Haggai"}, {"hag", "Haggai"}, {"hg", "Haggai"},
        {"zechariah", "Zechariah"}, {"zech", "Zechariah"}, {"zec", "Zechariah"}, {"zc", "Zechariah"},
        {"malachi", "Malachi"}, {"mal", "Malachi"}, {"ma", "Malachi"},
        
        // New Testament
        {"matthew", "Matthew"}, {"matt", "Matthew"}, {"mat", "Matthew"}, {"mt", "Matthew"},
        {"mark", "Mark"}, {"mrk", "Mark"}, {"mar", "Mark"}, {"mk", "Mark"}, {"mr", "Mark"},
        {"luke", "Luke"}, {"luk", "Luke"}, {"lk", "Luke"},
        {"john", "John"}, {"jhn", "John"}, {"joh", "John"}, {"jn", "John"},
        {"acts", "Acts"}, {"act", "Acts"}, {"ac", "Acts"},
        {"romans", "Romans"}, {"rom", "Romans"}, {"ro", "Romans"}, {"rm", "Romans"},
        {"1 corinthians", "1 Corinthians"}, {"1cor", "1 Corinthians"}, {"1 co", "1 Corinthians"}, {"1co", "1 Corinthians"}, {"I corinthians", "I Corinthians"},
        {"2 corinthians", "2 Corinthians"}, {"2cor", "2 Corinthians"}, {"2 co", "2 Corinthians"}, {"2co", "2 Corinthians"}, {"II corinthians", "II Corinthians"},
        {"galatians", "Galatians"}, {"gal", "Galatians"}, {"ga", "Galatians"},
        {"ephesians", "Ephesians"}, {"eph", "Ephesians"}, {"ep", "Ephesians"},
        {"philippians", "Philippians"}, {"phil", "Philippians"}, {"phi", "Philippians"}, {"php", "Philippians"},
        {"colossians", "Colossians"}, {"col", "Colossians"}, {"co", "Colossians"},
        {"1 thessalonians", "1 Thessalonians"}, {"1thess", "1 Thessalonians"}, {"1 th", "1 Thessalonians"}, {"1th", "1 Thessalonians"}, {"I thessalonians", "I Thessalonians"},
        {"2 thessalonians", "2 Thessalonians"}, {"2thess", "2 Thessalonians"}, {"2 th", "2 Thessalonians"}, {"2th", "2 Thessalonians"}, {"II thessalonians", "II Thessalonians"},
        {"1 timothy", "1 Timothy"}, {"1tim", "1 Timothy"}, {"1 ti", "1 Timothy"}, {"1ti", "1 Timothy"}, {"I timothy", "I Timothy"},
        {"2 timothy", "2 Timothy"}, {"2tim", "2 Timothy"}, {"2 ti", "2 Timothy"}, {"2ti", "2 Timothy"}, {"II timothy", "II Timothy"},
        {"titus", "Titus"}, {"tit", "Titus"}, {"ti", "Titus"},
        {"philemon", "Philemon"}, {"phile", "Philemon"}, {"phm", "Philemon"}, {"pm", "Philemon"},
        {"hebrews", "Hebrews"}, {"heb", "Hebrews"}, {"he", "Hebrews"},
        {"james", "James"}, {"jas", "James"}, {"jam", "James"}, {"jm", "James"},
        {"1 peter", "1 Peter"}, {"1pet", "1 Peter"}, {"1 pe", "1 Peter"}, {"1pe", "1 Peter"}, {"1pt", "1 Peter"},
        {"2 peter", "2 Peter"}, {"2pet", "2 Peter"}, {"2 pe", "2 Peter"}, {"2pe", "2 Peter"}, {"2pt", "2 Peter"},
        {"1 john", "1 John"}, {"1jn", "1 John"}, {"1 jo", "1 John"}, {"1st john", "1st John"},
        {"2 john", "2 John"}, {"2jn", "2 John"}, {"2 jo", "2 John"}, {"2nd john", "2nd John"},
        {"3 john", "3 John"}, {"3jn", "3 John"}, {"3 jo", "3 John"}, {"3rd john", "3rd John"},
        {"jude", "Jude"}, {"jud", "Jude"}, {"jd", "Jude"},
        {"revelation", "Revelation"}, {"rev", "Revelation"}, {"re", "Revelation"}, {"rv", "Revelation"}
    };

    // Ordered list of Bible books for range calculations
    const std::vector<std::string> BibleBookOrder = {
        "Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy", "Joshua", "Judges", "Ruth",
        "1 Samuel", "2 Samuel", "1 Kings", "2 Kings", "1 Chronicles", "2 Chronicles", "Ezra", "Nehemiah",
        "Esther", "Job", "Psalms", "Proverbs", "Ecclesiastes", "Song of Solomon", "Isaiah", "Jeremiah",
        "Lamentations", "Ezekiel", "Daniel", "Hosea", "Joel", "Amos", "Obadiah", "Jonah", "Micah",
        "Nahum", "Habakkuk", "Zephaniah", "Haggai", "Zechariah", "Malachi",
        "Matthew", "Mark", "Luke", "John", "Acts", "Romans", "1 Corinthians", "2 Corinthians",
        "Galatians", "Ephesians", "Philippians", "Colossians", "1 Thessalonians", "2 Thessalonians",
        "1 Timothy", "2 Timothy", "Titus", "Philemon", "Hebrews", "James", "1 Peter", "2 Peter",
        "1 John", "2 John", "3 John", "Jude", "Revelation"
    };

    std::string NormalizeBookName(const std::string& bookName) {
        std::string normalized = bookName;
        
        // Convert to lowercase for case-insensitive matching
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        
        // Trim whitespace
        normalized.erase(0, normalized.find_first_not_of(" \t\n\r\f\v"));
        normalized.erase(normalized.find_last_not_of(" \t\n\r\f\v") + 1);
        
        // Look up in aliases map
        auto it = BookAliases.find(normalized);
        if (it != BookAliases.end()) {
            return it->second;
        }
        
        // If not found in aliases, try exact match (case-insensitive)
        for (const auto& book : BibleBookOrder) {
            std::string bookLower = book;
            std::transform(bookLower.begin(), bookLower.end(), bookLower.begin(), ::tolower);
            if (bookLower == normalized) {
                return book;
            }
        }
        
        return ""; // Not found
    }

    bool IsValidBook(const std::string& bookName) {
        std::string normalized = NormalizeBookName(bookName);
        return !normalized.empty();
    }

    std::vector<std::string> GetBookNames() {
        return BibleBookOrder;
    }

    int GetBookIndex(const std::string& bookName) {
        std::string normalized = NormalizeBookName(bookName);
        if (normalized.empty()) return -1;
        
        for (size_t i = 0; i < BibleBookOrder.size(); ++i) {
            if (BibleBookOrder[i] == normalized) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
}
