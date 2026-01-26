#pragma once
#ifndef VerseRetrieveInterface_H
#define VerseRetrieveInterface_H

#include <sstream>
#include <vector>
#include "StringExtensions.h"
#include "Bible.h"
#include "tinyxml2.h"

#pragma comment(lib, "urlmon.lib")

using namespace tinyxml2;

class VerseRetrieveInterface
{
    std::string UserInput;
    std::string BibleVersion;
    std::vector<Bible::BibleReference> references;

    bool parseBibleReference();
    bool parseSingleReference(const std::string& refStr);
    std::vector<std::string> splitMultipleReferences(const std::string& input);
    XMLElement* ParseXMLBible(XMLElement* node, const std::string& targetAttributeValue);
    std::string prepareResult(const std::string& result);
    std::string addVerseNumbers(const std::string& text);
    std::string addNewLinesBetweenChapters(const std::string& text);
    std::string AddNewLinesBetweenBooks(const std::string& text);
    std::string cleanXmlMarkers(const std::string& text);

    std::string getVersesFromChapter(XMLElement* chapterNode, const std::string& startVerse, const std::string& endVerse);
    std::string getMultipleVerses(XMLElement* chapterNode, const std::string& versesList);
    std::string getChaptersFromBook(XMLElement* bookNode, const std::string& startChapter, const std::string& endChapter);
    void LogMessage(const std::string& message);

public:	
    std::string Log;
    std::string LastError;
    std::string ReferenceText;
    std::string VerseText;
    std::string GetLog() const { return Log; }
    VerseRetrieveInterface(const std::string& selectedText, const std::string& version);
    bool GetVerseText();
    ~VerseRetrieveInterface();
};

#endif