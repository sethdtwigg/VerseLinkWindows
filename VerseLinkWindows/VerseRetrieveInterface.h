#pragma once
#ifndef VerseRetrieveInterface_H
#define VerseRetrieveInterface_H

#include <sstream>
#include <vector>
#include "StringExtensions.h"
#include "Bible.h"
#include "tinyxml2.h"

using namespace tinyxml2;

class VerseRetrieveInterface
{
    std::string UserInput;
    std::string BibleVersion;
    std::vector<Bible::BibleReference> references;

    bool parseBibleReference();
    bool parseSingleReference(const std::string& refStr);
    std::vector<std::string> splitMultipleReferences(const std::string& input);
    const XMLElement* ParseXMLBible(const XMLElement* node, const std::string& targetAttributeValue);
    std::string prepareResult(const std::string& result);

    std::string getVersesFromChapter(const XMLElement* chapterNode, const std::string& startVerse, const std::string& endVerse);
    std::string getMultipleVerses(const XMLElement* chapterNode, const std::string& versesList);
    void LogMessage(const std::string& message);

public:	
    std::string Log;
    std::string LastError;
    std::string ReferenceText;
    std::string VerseText;
    std::string GetLog() const { return Log; }
    const std::vector<Bible::BibleReference>& ParsedReferences() const { return references; }
    VerseRetrieveInterface(const std::string& selectedText, const std::string& version);
    bool GetVerseText();
    ~VerseRetrieveInterface();
};

#endif