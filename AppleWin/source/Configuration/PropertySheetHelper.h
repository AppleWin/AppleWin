#pragma once
#include "PropertySheetDefs.h"

class CPropertySheetHelper
{
public:
	CPropertySheetHelper() :
		m_LastPage(PG_CONFIG),
		m_UIControlFreezeDlgButton(UI_UNDEFINED),
		m_UIControlCloneDropdownMenu(UI_UNDEFINED),
		m_bSSNewFilename(false)
	{}
	virtual ~CPropertySheetHelper(){}

	bool IsOkToRestart(HWND window);
	void FillComboBox(HWND window, int controlid, LPCTSTR choices, int currentchoice);
	void SaveComputerType(eApple2Type NewApple2Type);
	void SetSlot4(SS_CARDTYPE NewCardType);
	void SetSlot5(SS_CARDTYPE NewCardType);
	string BrowseToFile(HWND hWindow, TCHAR* pszTitle, TCHAR* REGVALUE,TCHAR* FILEMASKS);
	void SaveStateUpdate();
	void GetDiskBaseNameWithAWS(TCHAR* pszFilename);
	int SaveStateSelectImage(HWND hWindow, TCHAR* pszTitle, bool bSave);

	PAGETYPE GetLastPage(void) { return m_LastPage; }
	void SetLastPage(PAGETYPE page) { m_LastPage = page; }
	UICONTROLSTATE GetUIControlFreezeDlgButton(void) { return m_UIControlFreezeDlgButton; }
	void SetUIControlFreezeDlgButton(UICONTROLSTATE state) { m_UIControlFreezeDlgButton = state; }
	UICONTROLSTATE GetUIControlCloneDropdownMenu(void) { return m_UIControlCloneDropdownMenu; }
	void SetUIControlCloneDropdownMenu(UICONTROLSTATE state) { m_UIControlCloneDropdownMenu = state; }
	char* GetSSNewFilename(void) { return &m_szSSNewFilename[0]; }
	void ClearSSNewDirectory(void) { m_szSSNewDirectory[0] = 0; }

private:
	PAGETYPE m_LastPage;
	UICONTROLSTATE m_UIControlFreezeDlgButton;
	UICONTROLSTATE m_UIControlCloneDropdownMenu;
	char m_szNewFilename[MAX_PATH];
	bool m_bSSNewFilename;
	char m_szSSNewDirectory[MAX_PATH];
	char m_szSSNewFilename[MAX_PATH];
};
