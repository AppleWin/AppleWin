#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "PageConfigTfe.h"
class CPropertySheetHelper;

class CPageConfig : public IPropertySheetPage
{
public:
	CPageConfig(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_CONFIG),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uAfterClose(0)
	{
		CPageConfig::ms_this = this;
	}
	virtual ~CPageConfig(){}

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window);
	virtual void DlgCANCEL(HWND window){}

private:
	eApple2Type GetApple2Type(DWORD NewMenuItem);
	void EnableTrackbar(HWND window, BOOL enable);
	void ui_tfe_settings_dialog(HWND hwnd);

	static CPageConfig* ms_this;
	static const TCHAR m_ComputerChoices[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uAfterClose;
	CPageConfigTfe m_PageConfigTfe;
};
