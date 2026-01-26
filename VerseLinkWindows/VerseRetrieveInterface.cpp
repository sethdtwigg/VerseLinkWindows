#include "VerseRetrieveInterface.h"
#include "ConfigManager.h"
#include <algorithm>
#include <regex>
#include <sstream>

VerseRetrieveInterface::VerseRetrieveInterface(const std::string& selectedText, const std::string& version)
{
    UserInput = selectedText;
    BibleVersion = version;
    references.clear();
    
    LogMessage("Initializing VerseRetrieveInterface with: " + selectedText);
    
    if (!parseBibleReference()) {
        LastError = "Failed to parse Bible Reference: " + selectedText;
        LogMessage(LastError);
    }
}

void VerseRetrieveInterface::LogMessage(const std::string& message) {
    Log += message + "\n";
}

std::vector<std::string> VerseRetrieveInterface::splitMultipleReferences(const std::string& input) {
    std::vector<std::string> references;
    
    // Split on semicolons, commas, or "and" (case insensitive)
    std::regex separator(R"(;|,|\s+and\s+|\s+AND\s+)", std::regex_constants::icase);
    std::sregex_token_iterator iter(input.begin(), input.end(), separator, -1);
    std::sregex_token_iterator end;
    
    for (; iter != end; ++iter) {
        std::string ref = iter->str();
        // Trim whitespace
        ref.erase(0, ref.find_first_not_of(" \t\n\r\f\v"));
        ref.erase(ref.find_last_not_of(" \t\n\r\f\v") + 1);
        
        if (!ref.empty()) {
            references.push_back(ref);
        }
    }
    
    return references;
}

bool VerseRetrieveInterface::parseSingleReference(const std::string& refStr) {
    Bible::BibleReference br;
    br.Version = BibleVersion;
    
    LogMessage("Parsing reference: " + refStr);
    
    // Enhanced regex patterns for Bible references
    std::vector<std::regex> patterns = {
        // Cross-chapter verse range with book repeated: "Romans 8:28 - Romans 9:1"
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):(\d+)\s*[-–—]\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):(\d+)\s*$)", std::regex_constants::icase),
        // Cross-chapter verse range: "Romans 8:28-9:1"
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):(\d+)\s*[-–—]\s*(\d+):(\d+)\s*$)", std::regex_constants::icase),
        // Book range (e.g., "Jonah 1 - Micah 1")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+)\s*[-–—]\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+)\s*$)", std::regex_constants::icase),
        // Book range without chapters (e.g., "Genesis - Exodus")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s*[-–—]\s*([0-9]*\s*[a-zA-Z]+)\s*$)", std::regex_constants::icase),
        // Chapter range (e.g., "John 1-2")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+)\s*[-–—]\s*(\d+)\s*$)", std::regex_constants::icase),
        // Book Chapter:Verse-EndVerse (e.g., "Romans 8:1-5")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):(\d+)\s*[-–—]\s*(\d+)\s*$)", std::regex_constants::icase),
        // Multiple verses (e.g., "John 3:16,18,20")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):([\d,]+)\s*$)", std::regex_constants::icase),
        // Book Chapter:Verse (e.g., "John 3:16")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+):(\d+)\s*$)", std::regex_constants::icase),
        // Book Chapter (e.g., "Genesis 1")
        std::regex(R"(^\s*([0-9]*\s*[a-zA-Z]+)\s+(\d+)\s*$)", std::regex_constants::icase)
    };
    
    for (size_t i = 0; i < patterns.size(); ++i) {
        std::smatch match;
        if (std::regex_match(refStr, match, patterns[i])) {
            
            if (i == 0) { // Cross-chapter verse range with book repeated: "Romans 8:28 - Romans 9:1"
                std::string book1Name = Bible::NormalizeBookName(match[1].str());
                std::string chapter1 = match[2].str();
                std::string verse1 = match[3].str();
                std::string book2Name = Bible::NormalizeBookName(match[4].str());
                std::string chapter2 = match[5].str();
                std::string verse2 = match[6].str();
                
                if (book1Name.empty() || book2Name.empty()) {
                    LogMessage("Unknown book in cross-chapter range: " + match[1].str() + " - " + match[4].str());
                    return false;
                }
                
                br.BookName = book1Name;
                br.ChapterNumber = chapter1;
                br.VerseNumber = verse1;
                br.EndBookName = book2Name;
                br.EndChapterNumber = chapter2;
                br.EndVerseNumber = verse2;
                br.Type = Bible::ReferenceType::VERSE_RANGE;
                br.IsRange = true;
                LogMessage("Parsed cross-chapter verse range: " + book1Name + " " + chapter1 + ":" + verse1 + " - " + book2Name + " " + chapter2 + ":" + verse2);
                
            } else if (i == 1) { // Cross-chapter verse range: "Romans 8:28-9:1"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                std::string chapter1 = match[2].str();
                std::string verse1 = match[3].str();
                std::string chapter2 = match[4].str();
                std::string verse2 = match[5].str();
                
                if (bookName.empty()) {
                    LogMessage("Unknown book in cross-chapter range: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = chapter1;
                br.VerseNumber = verse1;
                br.EndBookName = bookName; // Same book
                br.EndChapterNumber = chapter2;
                br.EndVerseNumber = verse2;
                br.Type = Bible::ReferenceType::VERSE_RANGE;
                br.IsRange = true;
                LogMessage("Parsed cross-chapter verse range: " + bookName + " " + chapter1 + ":" + verse1 + "-" + chapter2 + ":" + verse2);
                
            } else if (i == 2) { // Book range with chapters: "Jonah 1 - Micah 1"
                std::string book1Name = Bible::NormalizeBookName(match[1].str());
                std::string chapter1 = match[2].str();
                std::string book2Name = Bible::NormalizeBookName(match[3].str());
                std::string chapter2 = match[4].str();
                
                if (book1Name.empty() || book2Name.empty()) {
                    LogMessage("Unknown book in range: " + match[1].str() + " - " + match[3].str());
                    return false;
                }
                
                br.BookName = book1Name;
                br.EndBookName = book2Name;
                br.ChapterNumber = chapter1;
                br.EndChapterNumber = chapter2;
                br.Type = Bible::ReferenceType::BOOK_RANGE;
                br.IsRange = true;
                LogMessage("Parsed book range: " + book1Name + " " + chapter1 + " - " + book2Name + " " + chapter2);
                
            } else if (i == 3) { // Book range without chapters: "Genesis - Exodus"
                std::string book1Name = Bible::NormalizeBookName(match[1].str());
                std::string book2Name = Bible::NormalizeBookName(match[2].str());
                
                if (book1Name.empty() || book2Name.empty()) {
                    LogMessage("Unknown book in range: " + match[1].str() + " - " + match[2].str());
                    return false;
                }
                
                br.BookName = book1Name;
                br.EndBookName = book2Name;
                br.Type = Bible::ReferenceType::BOOK_RANGE;
                br.IsRange = true;
                LogMessage("Parsed book range: " + book1Name + " - " + book2Name);
                
            } else if (i == 4) { // Chapter range: "John 1-2"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                std::string chapter1 = match[2].str();
                std::string chapter2 = match[3].str();
                
                if (bookName.empty()) {
                    LogMessage("Unknown book: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = chapter1;
                br.EndChapterNumber = chapter2;
                br.Type = Bible::ReferenceType::CHAPTER_RANGE;
                br.IsRange = true;
                LogMessage("Parsed chapter range: " + bookName + " " + chapter1 + "-" + chapter2);
                
            } else if (i == 5) { // Verse range: "Romans 8:1-5"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                
                if (bookName.empty()) {
                    LogMessage("Unknown book: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = match[2].str();
                br.VerseNumber = match[3].str();
                br.EndVerseNumber = match[4].str();
                br.Type = Bible::ReferenceType::VERSE_RANGE;
                br.IsRange = true;
                LogMessage("Parsed verse range: " + bookName + " " + br.ChapterNumber + ":" + br.VerseNumber + "-" + br.EndVerseNumber);
                
            } else if (i == 6) { // Multiple verses: "John 3:16,18,20"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                
                if (bookName.empty()) {
                    LogMessage("Unknown book: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = match[2].str();
                br.VerseNumber = match[3].str();
                br.Type = Bible::ReferenceType::MULTIPLE_VERSES;
                br.IsRange = false;
                LogMessage("Parsed multiple verses: " + bookName + " " + br.ChapterNumber + ":" + br.VerseNumber);
                
            } else if (i == 7) { // Single verse: "John 3:16"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                
                if (bookName.empty()) {
                    LogMessage("Unknown book: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = match[2].str();
                br.VerseNumber = match[3].str();
                br.Type = Bible::ReferenceType::SINGLE_VERSE;
                br.IsRange = false;
                LogMessage("Parsed single verse: " + bookName + " " + br.ChapterNumber + ":" + br.VerseNumber);
                
            } else if (i == 8) { // Chapter only: "Genesis 1"
                std::string bookName = Bible::NormalizeBookName(match[1].str());
                
                if (bookName.empty()) {
                    LogMessage("Unknown book: " + match[1].str());
                    return false;
                }
                
                br.BookName = bookName;
                br.ChapterNumber = match[2].str();
                br.Type = Bible::ReferenceType::CHAPTER_ONLY;
                br.IsRange = false;
                LogMessage("Parsed chapter only: " + bookName + " " + br.ChapterNumber);
            }
            
            references.push_back(br);
            return true;
        }
    }
    
    LogMessage("No pattern matched for: " + refStr);
    return false;
}

bool VerseRetrieveInterface::parseBibleReference()
{
    if (UserInput.empty()) {
        LogMessage("Empty input provided");
        return false;
    }
    
    // Try to split multiple references first
    auto refStrings = splitMultipleReferences(UserInput);
    
    bool success = false;
    for (const auto& refStr : refStrings) {
        if (parseSingleReference(refStr)) {
            success = true;
        }
    }
    
    if (success) {
        LogMessage("Successfully parsed " + std::to_string(references.size()) + " reference(s)");
    } else {
        LogMessage("Failed to parse any references from: " + UserInput);
    }
    
    return success;
}

XMLElement* VerseRetrieveInterface::ParseXMLBible(XMLElement* node, const std::string& targetAttributeValue) {
    if (!node) return nullptr;
    
    for (XMLElement* currentNode = node; currentNode; currentNode = currentNode->NextSiblingElement()) {
        const char* attributeValue = currentNode->Attribute("n");
        if (attributeValue && std::string(attributeValue) == targetAttributeValue) {
            return currentNode;
        }
    }
    return nullptr;
}

std::string VerseRetrieveInterface::getVersesFromChapter(XMLElement* chapterNode, const std::string& startVerse, const std::string& endVerse) {
    std::string result;
    
    if (!chapterNode) {
        LogMessage("Invalid chapter node");
        return result;
    }
    
    try {
        int start = std::stoi(startVerse);
        int end = endVerse.empty() ? start : std::stoi(endVerse);
        
        LogMessage("Getting verses " + std::to_string(start) + " to " + std::to_string(end));
        
        // Check if verse numbering is enabled
        auto& config = ConfigManager::getInstance();
        bool includeVerseNumbers = config.includeVerseNumbers();
        
        XMLElement* verseNode = chapterNode->FirstChildElement();
        while (verseNode) {
            const char* verseNum = verseNode->Attribute("n");
            if (verseNum) {
                int verse = std::stoi(verseNum);
                if (verse >= start && verse <= end) {
                    const char* text = verseNode->GetText();
                    if (text) {
                        if (!result.empty()) result += " ";
                        
                        // Add verse number if enabled
                        if (includeVerseNumbers) {
                            result += std::string(verseNum) + " ";
                        }
                        
                        result += std::string(text);
                    }
                }
                if (verse > end) break;
            }
            verseNode = verseNode->NextSiblingElement();
        }
    } catch (const std::exception& e) {
        LogMessage("Error parsing verse numbers: " + std::string(e.what()));
    }
    
    LogMessage("Retrieved " + std::to_string(result.length()) + " characters");
    return result;
}

std::string VerseRetrieveInterface::getMultipleVerses(XMLElement* chapterNode, const std::string& versesList) {
    std::string result;
    
    if (!chapterNode) {
        LogMessage("Invalid chapter node for multiple verses");
        return result;
    }
    
    try {
        // Parse comma-separated verse numbers
        std::vector<std::string> verses;
        std::stringstream ss(versesList);
        std::string verseText;
            
        LogMessage("Getting multiple verses: " + versesList);
        
        for (const auto& verseNum : verses) {
            std::string verseText = getVersesFromChapter(chapterNode, verseNum, verseNum);
            if (!verseText.empty()) {
                if (!result.empty()) result += " ";
                result += verseText;
            }
        }
    } catch (const std::exception& e) {
        LogMessage("Error parsing multiple verses: " + std::string(e.what()));
    }
    
    return result;
}

std::string VerseRetrieveInterface::getChaptersFromBook(XMLElement* bookNode, const std::string& startChapter, const std::string& endChapter) {
    std::string result;
    
    if (!bookNode) {
        LogMessage("Invalid book node");
        return result;
    }
    
    try {
        int start = std::stoi(startChapter);
        int end = endChapter.empty() ? start : std::stoi(endChapter);
        
        LogMessage("Getting chapters " + std::to_string(start) + " to " + std::to_string(end));
        
        XMLElement* chapterNode = bookNode->FirstChildElement();
        int currentChapter = 1;
        
        while (chapterNode && currentChapter <= end) {
            const char* chapterNum = chapterNode->Attribute("n");
            if (chapterNum) {
                currentChapter = std::stoi(chapterNum);
                if (currentChapter >= start && currentChapter <= end) {
                    // Get all verses in this chapter
                    std::string chapterText = getVersesFromChapter(chapterNode, "1", "999");
                    if (!chapterText.empty()) {
                        if (!result.empty()) result += " ";
                        result += chapterText;
                    }
                }
            }
            chapterNode = chapterNode->NextSiblingElement();
            currentChapter++;
        }
    } catch (const std::exception& e) {
        LogMessage("Error parsing chapter numbers: " + std::string(e.what()));
    }
    
    return result;
}

bool VerseRetrieveInterface::GetVerseText() {
    try {
        std::string resultString = "";

        tinyxml2::XMLDocument doc;
        if (doc.LoadFile(BibleVersion.c_str()) != XML_SUCCESS) {
           LogMessage("Error loading XML file: " + BibleVersion);
            LastError = "Could not load Bible XML file: " + BibleVersion;
            return false;
        }

        LogMessage("Successfully loaded XML file");

        auto& config = ConfigManager::getInstance();
        std::string referenceText;

        // Add the reference to the result once, before the loop
        if (!references.empty()) {
            const auto& firstRef = references[0];
            if (config.dynamicReference() && !firstRef.ChapterNumber.empty() && !firstRef.VerseNumber.empty()) {
                referenceText = firstRef.BookName + " " + firstRef.ChapterNumber + ":" + firstRef.VerseNumber;
            } else {
                referenceText = firstRef.BookName;
                if (!firstRef.ChapterNumber.empty()) {
                    referenceText += " " + firstRef.ChapterNumber;
                    if (!firstRef.VerseNumber.empty()) {
                        referenceText += ":" + firstRef.VerseNumber;
                        if (!firstRef.EndVerseNumber.empty()) {
                            if (!firstRef.EndChapterNumber.empty() && firstRef.EndChapterNumber != firstRef.ChapterNumber) {
                                if (!firstRef.EndBookName.empty() && firstRef.EndBookName != firstRef.BookName) {
                                    referenceText += " - " + firstRef.EndBookName + " " + firstRef.EndChapterNumber + ":" + firstRef.EndVerseNumber;
                                } else {
                                    referenceText += "-" + firstRef.EndChapterNumber + ":" + firstRef.EndVerseNumber;
                                }
                            } else {
                                referenceText += "-" + firstRef.EndVerseNumber;
                            }
                        } else if (!firstRef.EndChapterNumber.empty()) {
                            referenceText += "-" + firstRef.EndChapterNumber;
                        }
                    } else if (!firstRef.EndChapterNumber.empty()) {
                        referenceText += "-" + firstRef.EndChapterNumber;
                    }
                } else if (!firstRef.EndChapterNumber.empty()) {
                    referenceText += "-" + firstRef.EndChapterNumber;
                }
            }
        }

        ReferenceText = referenceText;

        if (!config.includeReferenceInReplacement() && !referenceText.empty()) {
            if (config.referenceOnFirstLine()) {
                resultString = referenceText + "\n";
            } else {
                resultString = referenceText + " ";
            }
        } else if (config.referenceOnFirstLine()) {
            resultString = "\n";
        }

        auto appendWithSeparator = [](std::string& target, const std::string& addition, const std::string& separator) {
            if (addition.empty()) return;
            if (!target.empty() && !separator.empty()) {
                target += separator;
            }
            target += addition;
        };

        auto getBookText = [&](XMLElement* bookNode, int startChapter, int endChapter) -> std::string {
            std::string bookText;
            if (!bookNode) return bookText;

            for (int chap = startChapter; chap <= endChapter; ++chap) {
                auto chapterNode = ParseXMLBible(bookNode->FirstChildElement(), std::to_string(chap));
                if (!chapterNode) {
                    continue;
                }

                std::string chapterText = getVersesFromChapter(chapterNode, "1", "999");
                if (!chapterText.empty()) {
                    appendWithSeparator(bookText, chapterText, config.newLineBetweenChapters() ? "\n" : " ");
                }
            }

            return bookText;
        };

        for (const auto& ref : references) {
            std::string verseText;
            XMLElement* currentNode = doc.RootElement()->FirstChildElement();
            if (!currentNode) {
                LogMessage("No child elements found in XML root");
                continue;
            }

            if (ref.IsBookRange()) {
                LogMessage("Processing book range: " + ref.BookName + " to " + ref.EndBookName);
                
                // Get all books in the range
                auto bookNames = Bible::GetBookNames();
                int startIndex = Bible::GetBookIndex(ref.BookName);
                int endIndex = Bible::GetBookIndex(ref.EndBookName);
                
                if (startIndex == -1 || endIndex == -1) {
                    LogMessage("Invalid book range: " + ref.BookName + " to " + ref.EndBookName);
                    continue;
                }
                
                for (int i = startIndex; i <= endIndex; ++i) {
                    std::string currentBook = bookNames[i];
                    auto bookNode = ParseXMLBible(currentNode, currentBook);
                    if (!bookNode) {
                        LogMessage("Book not found: " + currentBook);
                        continue;
                    }

                    int startChap = 1;
                    int endChap = 999;
                    if (startIndex == endIndex) {
                        if (!ref.ChapterNumber.empty()) startChap = std::stoi(ref.ChapterNumber);
                        if (!ref.EndChapterNumber.empty()) endChap = std::stoi(ref.EndChapterNumber);
                    } else {
                        if (i == startIndex) {
                            if (!ref.ChapterNumber.empty()) startChap = std::stoi(ref.ChapterNumber);
                        } else if (i == endIndex) {
                            if (!ref.EndChapterNumber.empty()) endChap = std::stoi(ref.EndChapterNumber);
                        }
                    }

                    std::string bookText = getBookText(bookNode, startChap, endChap);

                    if (!bookText.empty()) {
                        appendWithSeparator(verseText, bookText, config.newLineBetweenBooks() ? "\n\n" : " ");
                    }
                }
                
            } else {
                // Handle single book references
                auto bookNode = ParseXMLBible(currentNode, ref.BookName);
                if (!bookNode) {
                    LogMessage("Book not found: " + ref.BookName);
                    continue;
                }
                
                if (ref.IsChapterRange()) {
                    // Handle chapter ranges (e.g., "John 1-2")
                    LogMessage("Processing chapter range: " + ref.BookName + " " + ref.ChapterNumber + "-" + ref.EndChapterNumber);
                    int startChap = std::stoi(ref.ChapterNumber);
                    int endChap = std::stoi(ref.EndChapterNumber);
                    verseText = getBookText(bookNode, startChap, endChap);
                    
                } else if (ref.IsVerseRange()) {
                    // Check if this is a cross-chapter verse range
                    if (!ref.EndChapterNumber.empty() && ref.EndChapterNumber != ref.ChapterNumber) {
                        // Cross-chapter verse range (e.g., "Romans 8:28-9:1")
                        LogMessage("Processing cross-chapter verse range: " + ref.BookName + " " + ref.ChapterNumber + ":" + ref.VerseNumber + " - " + ref.EndChapterNumber + ":" + ref.EndVerseNumber);
                        
                        // Get verses from first chapter (start verse to end of chapter)
                        auto startChapterNode = ParseXMLBible(bookNode->FirstChildElement(), ref.ChapterNumber);
                        if (startChapterNode) {
                            std::string startVerses = getVersesFromChapter(startChapterNode, ref.VerseNumber, "999");
                            if (!startVerses.empty()) {
                                appendWithSeparator(verseText, startVerses, "");
                            }
                        }
                        
                        // Get verses from intermediate chapters (if any)
                        int startChapter = std::stoi(ref.ChapterNumber);
                        int endChapter = std::stoi(ref.EndChapterNumber);
                        
                        for (int chapNum = startChapter + 1; chapNum < endChapter; ++chapNum) {
                            auto intermediateChapterNode = ParseXMLBible(bookNode->FirstChildElement(), std::to_string(chapNum));
                            if (intermediateChapterNode) {
                                std::string intermediateVerses = getVersesFromChapter(intermediateChapterNode, "1", "999");
                                if (!intermediateVerses.empty()) {
                                    appendWithSeparator(verseText, intermediateVerses, config.newLineBetweenChapters() ? "\n" : " ");
                                }
                            }
                        }
                        
                        // Get verses from last chapter (beginning to end verse)
                        auto endChapterNode = ParseXMLBible(bookNode->FirstChildElement(), ref.EndChapterNumber);
                        if (endChapterNode) {
                            std::string endVerses = getVersesFromChapter(endChapterNode, "1", ref.EndVerseNumber);
                            if (!endVerses.empty()) {
                                appendWithSeparator(verseText, endVerses, config.newLineBetweenChapters() ? "\n" : " ");
                            }
                        }
                        
                        LogMessage("Cross-chapter verse retrieval completed");
                        
                    } else {
                        // Same chapter verse range (e.g., "Romans 8:28-30")
                        LogMessage("Getting verse range " + ref.VerseNumber + "-" + ref.EndVerseNumber);
                        auto chapterNode = ParseXMLBible(bookNode->FirstChildElement(), ref.ChapterNumber);
                        if (!chapterNode) {
                            LogMessage("Chapter not found: " + ref.BookName + " " + ref.ChapterNumber);
                            continue;
                        }
                        verseText = getVersesFromChapter(chapterNode, ref.VerseNumber, ref.EndVerseNumber);
                    }
                        
                } else if (ref.IsMultipleVerses()) {
                    // Get multiple verses
                    LogMessage("Getting multiple verses " + ref.VerseNumber);
                    auto chapterNode = ParseXMLBible(bookNode->FirstChildElement(), ref.ChapterNumber);
                    if (!chapterNode) {
                        LogMessage("Chapter not found: " + ref.BookName + " " + ref.ChapterNumber);
                        continue;
                    }
                    verseText = getMultipleVerses(chapterNode, ref.VerseNumber);
                        
                } else {
                    // Get single verse
                    LogMessage("Getting single verse " + ref.VerseNumber);
                    auto chapterNode = ParseXMLBible(bookNode->FirstChildElement(), ref.ChapterNumber);
                    if (!chapterNode) {
                        LogMessage("Chapter not found: " + ref.BookName + " " + ref.ChapterNumber);
                        continue;
                    }
                    auto verseNode = ParseXMLBible(chapterNode->FirstChildElement(), ref.VerseNumber);
                    if (verseNode && verseNode->GetText()) {
                        verseText = verseNode->GetText();
                    } else {
                        LogMessage("Verse not found: " + ref.VerseNumber);
                    }
                }
            }
            
            if (!verseText.empty()) {
                if (!resultString.empty() && resultString.back() != '\n' && resultString.back() != ' ') {
                    resultString += " ";
                }
                resultString += verseText;
                LogMessage("Successfully retrieved verse text");
            }
        }
        
        VerseText = resultString.empty() ? "" : prepareResult(resultString);

        if (config.dynamicReference() && config.includeVerseNumbers() && !references.empty()) {
            const std::string firstVerseNumber = references[0].VerseNumber;
            if (!firstVerseNumber.empty() && !VerseText.empty()) {
                std::string prefix;
                std::string body = VerseText;
                if (!body.empty() && body.front() == '\n') {
                    prefix = "\n";
                    body.erase(0, 1);
                }

                const std::string token = firstVerseNumber + " ";
                if (body.rfind(token, 0) == 0) {
                    body.erase(0, token.size());
                    VerseText = prefix + body;
                }
            }
        }
        
        if (VerseText.empty()) {
            LogMessage("No verse text found for any references");
            return false;
        }
        
        LogMessage("Final verse text length: " + std::to_string(VerseText.length()));
        return true;
    }
    catch (const std::exception& e) {
        LastError = "Exception in GetVerseText: " + std::string(e.what());
        LogMessage(LastError);
        return false;
    }
}

std::string VerseRetrieveInterface::prepareResult(const std::string& result) {
    // Get configuration for verse formatting
    auto& config = ConfigManager::getInstance();
    
    // Clean up the result text
    std::string cleaned = result;

    // Remove extra whitespace
    std::regex multipleSpaces(R"([ \t\f\v]+)");
    cleaned = std::regex_replace(cleaned, multipleSpaces, " ");
    cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());

    // Trim whitespace
    cleaned.erase(0, cleaned.find_first_not_of(" \t"));
    cleaned.erase(cleaned.find_last_not_of(" \t") + 1);
    
    return cleaned;
}

std::string VerseRetrieveInterface::addVerseNumbers(const std::string& text) {
    // Verse numbers are now handled during retrieval, so return text unchanged
    return text;
}

std::string VerseRetrieveInterface::addNewLinesBetweenChapters(const std::string& text) {
    std::string result;
    std::istringstream iss(text);
    std::string word;
    int lastVerseNum = 0;
    
    while (iss >> word) {
        // Check if this word is a verse number
        if (!word.empty() && isdigit(word[0])) {
            try {
                int currentVerseNum = std::stoi(word);
                // If verse number reset to 1 after higher numbers, likely new chapter
                if (lastVerseNum > 0 && currentVerseNum == 1) {
                    result += "\n";  // Add newline for chapter break
                }
                lastVerseNum = currentVerseNum;
            } catch (...) {
                // Not a number, continue
            }
        }
        
        result += word + " ";
    }
    
    // Remove trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

std::string VerseRetrieveInterface::AddNewLinesBetweenBooks(const std::string& text) {
    std::string result;
    std::istringstream iss(text);
    std::string word;
    int lastVerseNum = 0;
    int resetCount = 0;
    
    while (iss >> word) {
        // Check if this word is a verse number
        if (!word.empty() && isdigit(word[0])) {
            try {
                int currentVerseNum = std::stoi(word);
                // If verse number reset to 1 after higher numbers, likely new chapter
                if (lastVerseNum > 0 && currentVerseNum == 1) {
                    resetCount++;
                    // If we've seen multiple resets, likely new book
                    if (resetCount > 1) {
                        result += "\n\n";  // Add double newline for book break
                    } else {
                        result += "\n";  // Add single newline for chapter break
                    }
                }
                lastVerseNum = currentVerseNum;
            } catch (...) {
                // Not a number, continue
            }
        }
        
        result += word + " ";
    }
    
    // Remove trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

VerseRetrieveInterface::~VerseRetrieveInterface()
{
}
