#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "Common.h"

class CPropertySheetHelper;

class CPageConfig : private IPropertySheetPage
{
public:
	CPageConfig(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_CONFIG),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uScrollLockToggle(0)
	{
		CPageConfig::ms_this = this;
	}
	virtual ~CPageConfig(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetScrollLockToggle(void) { return m_uScrollLockToggle; }
	void SetScrollLockToggle(UINT uValue) { m_uScrollLockToggle = uValue; }

	uint32_t GetVolumeMax(void) { return VOLUME_MAX; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	eApple2Type GetApple2Type(uint32_t NewMenuItem);
	void EnableTrackbar(HWND hWnd, BOOL enable);
	void ui_tfe_settings_dialog(HWND hWnd);
	void ResetAllToDefault(HWND hWnd);

	static CPageConfig* ms_this;
	static const char m_ComputerChoices[];

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uScrollLockToggle;
	COLORREF m_customColors[256];
};
