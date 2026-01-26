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
}

#endif