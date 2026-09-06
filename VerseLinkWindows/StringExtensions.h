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

	// Folds the Unicode punctuation that word processors, mail clients and web
	// pages substitute for plain ASCII back to ASCII, so reference parsing does
	// not have to care where the text was copied from. Word turns "8:1-5" into
	// "8:1<en dash>5"; without this the range patterns never match and the
	// hotkey silently does nothing.
	//
	// Dashes (U+2010..U+2015, U+2212) -> '-', the various fixed-width and
	// non-breaking spaces -> ' ', zero-width space and BOM are dropped, and
	// U+FFFD -> '-' so text that has already been mangled by a bad encoding
	// round-trip still parses. Input and output are both UTF-8.
	std::string NormalizeReferenceText(const std::string& utf8Text);
}
#endif