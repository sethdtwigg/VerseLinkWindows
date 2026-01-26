#pragma once
#ifndef StringExtensions_H
#define StringExtensions_H

#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

using namespace std;

namespace StringExtensions
{
	bool isEmptyString(string text);
	void ReplaceSubstring(string& str, const string& from, const string& to);
	vector<string> splitstring(const string& input, wchar_t separator);
};

#endif