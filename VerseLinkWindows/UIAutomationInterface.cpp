#include "UIAutomationInterface.h"

UIAutomationInterface::UIAutomationInterface()
{
    try {
        automation = nullptr;
        hr = CoInitialize(nullptr);
        if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&automation);
        if (SUCCEEDED(hr)) {
            focusedElement = nullptr;
            automation->GetFocusedElement(&focusedElement);
        }
        SelectedText = L"";
    }
    catch (const exception& e) {
        LastError = e.what();
    }
}

void UIAutomationInterface::GetSelectedText() {
    try {
        if (focusedElement) {
            IUnknown* objVal = nullptr;
            IUIAutomationTextPattern* pTextPattern = nullptr;
            IUIAutomationTextRangeArray* pTextRangeArray = nullptr;

            hr = focusedElement->GetCurrentPattern(UIA_TextPatternId, &objVal);
            if (SUCCEEDED(hr) && objVal != NULL) hr = objVal->QueryInterface(IID_IUIAutomationTextPattern, (void**)&pTextPattern);
            if (SUCCEEDED(hr) && pTextPattern != NULL) {

                hr = pTextPattern->GetSelection(&pTextRangeArray);
                if (SUCCEEDED(hr)) {
                    int rangeCount = 0;
                    pTextRangeArray->get_Length(&rangeCount);
                    for (int i = 0; i < rangeCount; ++i) {
                        IUIAutomationTextRange* pRange = nullptr;
                        pTextRangeArray->GetElement(i, &pRange);

                        BSTR bstrText = nullptr;
                        HRESULT hr = pRange->GetText(100, &bstrText);
                        if (SUCCEEDED(hr)) {
                            wstring currWString = wstring(bstrText, SysStringLen(bstrText));
                            SelectedText += currWString;
                        }
                        pRange->Release();
                    }
                }
            }
            if (pTextPattern != NULL) pTextPattern->Release();
            if (pTextRangeArray != NULL) pTextRangeArray->Release();
        }
    }
    catch (const exception& e) {
        LastError = e.what();
    }
}

void UIAutomationInterface::ReplaceSelectedText(wstring newText) {
    try {
        IUIAutomationValuePattern* pValuePattern = nullptr;
        hr = focusedElement->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pValuePattern);
        if (SUCCEEDED(hr) && pValuePattern != NULL) {
            BSTR bstrCurrValue = nullptr;
            hr = pValuePattern->get_CurrentValue(&bstrCurrValue);
            BSTR bstrNewValue = NULL;// WStringExtensions::ReplaceInBSTR(bstrCurrValue, SelectedText, newText);

            hr = pValuePattern->SetValue(bstrNewValue);
            if (SUCCEEDED(hr)) { bool succeeded = true; }
            ::SysFreeString(bstrNewValue);
            pValuePattern->Release();
        }
    }
    catch (const exception& e) {
        LastError = e.what();
    }
}

UIAutomationInterface::~UIAutomationInterface()
{
    if (focusedElement != NULL) focusedElement->Release();
    automation->Release();
    CoUninitialize();
}
