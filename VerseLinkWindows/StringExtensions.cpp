#include "StringExtensions.h"

namespace StringExtensions {
    bool isEmptyString(string text) {
        return (text.empty() || all_of(text.begin(), text.end(), ::isspace));
    }

    void ReplaceSubstring(string& str, const string& from, const string& to)
    {
        size_t startPos = 0;
        while ((startPos = str.find(from, startPos)) != string::npos) {
            str.replace(startPos, from.length(), to);
            startPos += to.length();
        }
    }

    vector<string> splitstring(const string& input, wchar_t separator) {
        vector<string> result;
        string token;
        for (char c : input) {
            if (c == static_cast<char>(separator)) {
                result.push_back(token);
                token.clear();
            }
            else {
                token += c;
            }
        }
        if (!token.empty()) {
            result.push_back(token);
        }
        return result;
    }
}
