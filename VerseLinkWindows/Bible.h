#pragma once
#ifndef Bible_H
#define Bible_H

#include <map>
#include <vector>
#include <string>

namespace Bible {
    enum class ReferenceType {
        SINGLE_VERSE,
        VERSE_RANGE,
        CHAPTER_ONLY,
        CHAPTER_RANGE,
        BOOK_RANGE,
        MULTIPLE_VERSES
    };

    struct BibleReference
    {
        std::string Version;
        std::string BookName;
        std::string EndBookName; // For book ranges
        std::string ChapterNumber;
        std::string EndChapterNumber; // For chapter ranges
        std::string VerseNumber;
        std::string EndVerseNumber; // For verse ranges
        ReferenceType Type = ReferenceType::SINGLE_VERSE;
        bool IsRange = false;
        
        // Helper methods
        bool IsBookRange() const { return Type == ReferenceType::BOOK_RANGE; }
        bool IsChapterRange() const { return Type == ReferenceType::CHAPTER_RANGE; }
        bool IsVerseRange() const { return Type == ReferenceType::VERSE_RANGE; }
        bool IsMultipleVerses() const { return Type == ReferenceType::MULTIPLE_VERSES; }
        bool IsChapterOnly() const { return Type == ReferenceType::CHAPTER_ONLY; }
    };

    // Book name aliases for flexible parsing
    extern const std::map<std::string, std::string> BookAliases;

    // Utility functions
    std::string NormalizeBookName(const std::string& bookName);
    bool IsValidBook(const std::string& bookName);
    std::vector<std::string> GetBookNames(); // Helper for book ranges
    int GetBookIndex(const std::string& bookName); // Helper for book ranges

    // Locates a Bible XML file. Search order:
    //   1. The given name/path verbatim (supports "Bibles/KJV.xml" style values)
    //   2. <dataPathOverride>/<fileName> and dataPathOverride itself, if provided
    //   3. Bibles/<fileName>, relative to the current working directory
    //   4. <fileName> relative to the current working directory
    //   5. Same as 2-4 but relative to the executable's directory
    // Returns an empty string if no existing regular file is found.
    std::string FindBibleFilePath(const std::string& fileName,
                                  const std::string& dataPathOverride = "");

    // Lists the *.xml Bible files available in the standard locations
    // (dataPathOverride, Bibles/ and the executable/working directories).
    // Returns file names only (e.g. "KJV.xml"), sorted, without duplicates.
    std::vector<std::string> FindAvailableBibleVersions(const std::string& dataPathOverride = "");
}

#endif