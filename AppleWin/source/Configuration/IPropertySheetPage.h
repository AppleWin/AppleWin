#pragma once

class IPropertySheetPage
{
public:
	IPropertySheetPage(){}
	virtual ~IPropertySheetPage(){}

protected:
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam) = 0;
	virtual void DlgOK(HWND window, UINT afterclose) = 0;
	virtual void DlgCANCEL(HWND window) = 0;
};
