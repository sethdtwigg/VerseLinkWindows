#pragma once
#ifndef UIAutomationInterface_H
#define UIAutomationInterface_H

#include <UIAutomation.h>
#include "StringExtensions.h"

class UIAutomationInterface
{
	IUIAutomation* automation;
	HRESULT hr;
	IUIAutomationElement* focusedElement;

public:
	wstring SelectedText;
	string LastError;
	UIAutomationInterface();
	void GetSelectedText();
	void ReplaceSelectedText(wstring newText);
	~UIAutomationInterface();
};

#endif