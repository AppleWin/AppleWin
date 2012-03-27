#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class CPropertySheetHelper;

class CPageAdvanced : public IPropertySheetPage
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

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetTheFreezesF8Rom(void){ return m_uTheFreezesF8Rom; }
	void SetTheFreezesF8Rom(UINT uValue){ m_uTheFreezesF8Rom = uValue; }

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window, UINT afterclose);
	virtual void DlgCANCEL(HWND window){}

private:
	eApple2Type GetCloneType(DWORD NewMenuItem);
	int GetCloneMenuItem(void);
	void InitFreezeDlgButton(HWND window);
	void InitCloneDropdownMenu(HWND window);

	static CPageAdvanced* ms_this;
	static const TCHAR m_CloneChoices[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uTheFreezesF8Rom;
};
