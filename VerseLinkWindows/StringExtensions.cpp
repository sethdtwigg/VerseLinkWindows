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