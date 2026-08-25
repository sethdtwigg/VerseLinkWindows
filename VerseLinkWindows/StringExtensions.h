#pragma once
#ifndef StringExtensions_H
#define StringExtensions_H

#include <string>
#include <windows.h>

namespace StringExtensions
{
	// Encoding conversions. All UI/clipboard/file text crossing the Win32
	// wide-char boundary must go through these; byte-wise widening such as
	// wstring(s.begin(), s.end()) corrupts every non-ASCII character.
	std::string WideToUtf8(const wchar_t* data, int length);
	std::string WideToUtf8(const std::wstring& text);
	std::wstring Utf8ToWide(const std::string& text);
	std::string AnsiToUtf8(const char* psz); // legacy ANSI codepage -> UTF-8
}
#endif