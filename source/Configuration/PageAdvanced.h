#pragma once

#include "PropertySheetDefs.h"
#include "IPropertySheetPage.h"
#include "Common.h"

class CPropertySheetHelper;

class CPageAdvanced : private IPropertySheetPage
{
public:
	CPageAdvanced(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_ADVANCED),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uTheFreezesF8Rom(0)
	{
		CPageAdvanced::ms_this = this;
	}
	virtual ~CPageAdvanced(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetTheFreezesF8Rom(void){ return m_uTheFreezesF8Rom; }
	void SetTheFreezesF8Rom(UINT uValue){ m_uTheFreezesF8Rom = uValue; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	eApple2Type GetCloneType(DWORD NewMenuItem);
	int GetCloneMenuItem(void);
	void InitFreezeDlgButton(HWND hWnd);
	void InitCloneDropdownMenu(HWND hWnd);

	static CPageAdvanced* ms_this;
	static const TCHAR m_CloneChoices[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uTheFreezesF8Rom;
};
