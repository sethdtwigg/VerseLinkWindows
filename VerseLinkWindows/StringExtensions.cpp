#include "StringExtensions.h"

namespace StringExtensions {
    std::string WideToUtf8(const wchar_t* data, int length) {
        if (!data || length <= 0) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, data, length, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return std::string();
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, data, length, &result[0], size, nullptr, nullptr);
        return result;
    }

    std::string WideToUtf8(const std::wstring& text) {
        return WideToUtf8(text.c_str(), static_cast<int>(text.size()));
    }

    std::wstring Utf8ToWide(const std::string& text) {
        if (text.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &result[0], size);
        return result;
    }

    namespace {
        // Decodes the UTF-8 sequence starting at `index`. Returns the code point
        // and sets `length` to the bytes consumed, or returns -1 for a malformed
        // or truncated sequence with `length` left at 1 so callers make progress.
        int DecodeUtf8(const std::string& text, size_t index, size_t& length) {
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            length = 1;
            if (lead < 0x80) return lead;

            size_t extra = 0;
            int codePoint = 0;
            if ((lead & 0xE0) == 0xC0)      { extra = 1; codePoint = lead & 0x1F; }
            else if ((lead & 0xF0) == 0xE0) { extra = 2; codePoint = lead & 0x0F; }
            else if ((lead & 0xF8) == 0xF0) { extra = 3; codePoint = lead & 0x07; }
            else return -1; // stray continuation byte or invalid lead

            if (index + extra >= text.size()) return -1;
            for (size_t offset = 1; offset <= extra; ++offset) {
                const unsigned char cont = static_cast<unsigned char>(text[index + offset]);
                if ((cont & 0xC0) != 0x80) return -1;
                codePoint = (codePoint << 6) | (cont & 0x3F);
            }

            length = extra + 1;
            return codePoint;
        }
    }

    std::string NormalizeReferenceText(const std::string& utf8Text) {
        std::string result;
        result.reserve(utf8Text.size());

        for (size_t i = 0; i < utf8Text.size(); ) {
            size_t length = 1;
            const int codePoint = DecodeUtf8(utf8Text, i, length);

            if (codePoint < 0) {
                // Not valid UTF-8; pass the byte through untouched rather than
                // guessing, so nothing is silently corrupted.
                result += utf8Text[i];
                i += 1;
                continue;
            }

            if (codePoint < 0x80) {
                result += static_cast<char>(codePoint);
            } else if ((codePoint >= 0x2010 && codePoint <= 0x2015) ||
                       codePoint == 0x2212 || codePoint == 0xFFFD) {
                // hyphen, non-breaking hyphen, figure/en/em dash, horizontal bar,
                // minus sign, and the replacement character a bad round-trip leaves
                result += '-';
            } else if (codePoint == 0x00A0 || (codePoint >= 0x2000 && codePoint <= 0x200A) ||
                       codePoint == 0x202F || codePoint == 0x205F || codePoint == 0x3000) {
                result += ' ';
            } else if (codePoint == 0x200B || codePoint == 0xFEFF) {
                // zero-width space / BOM: drop entirely
            } else {
                result.append(utf8Text, i, length);
            }

            i += length;
        }

        return result;
    }

    std::string AnsiToUtf8(const char* psz) {
        if (!psz) return std::string();
        int wideLength = MultiByteToWideChar(CP_ACP, 0, psz, -1, nullptr, 0);
        if (wideLength <= 0) return std::string();
        std::wstring wide(wideLength, L'\0');
        MultiByteToWideChar(CP_ACP, 0, psz, -1, &wide[0], wideLength);
        while (!wide.empty() && wide.back() == L'\0') wide.pop_back();
        return WideToUtf8(wide);
    }
}