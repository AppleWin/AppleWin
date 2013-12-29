#pragma once
#include "PropertySheetDefs.h"
#include "Config.h"

class CPropertySheetHelper
{
public:
	CPropertySheetHelper() :
		m_LastPage(PG_CONFIG),
		m_bmPages(0),
		m_bSSNewFilename(false),
		m_bDoBenchmark(false)
	{}
	virtual ~CPropertySheetHelper(){}

	void FillComboBox(HWND window, int controlid, LPCTSTR choices, int currentchoice);
	void SetSlot4(SS_CARDTYPE NewCardType);
	void SetSlot5(SS_CARDTYPE NewCardType);
	string BrowseToFile(HWND hWindow, TCHAR* pszTitle, TCHAR* REGVALUE,TCHAR* FILEMASKS);
	void SaveStateUpdate();
	void GetDiskBaseNameWithAWS(TCHAR* pszFilename);
	int SaveStateSelectImage(HWND hWindow, TCHAR* pszTitle, bool bSave);
	void PostMsgAfterClose(HWND hWnd, PAGETYPE page);

	void ResetPageMask(void) { m_bmPages = 0; }	// Req'd because cancelling doesn't clear the page-mask
	PAGETYPE GetLastPage(void) { return m_LastPage; }
	void SetLastPage(PAGETYPE page)
	{
		m_LastPage = page;
		m_bmPages |= 1<<(UINT32)page;
	}

	void SaveCurrentConfig(void);
	char* GetSSNewFilename(void) { return &m_szSSNewFilename[0]; }
	void ClearSSNewDirectory(void) { m_szSSNewDirectory[0] = 0; }
//	const CConfigNeedingRestart& GetConfigOld(void) { return m_ConfigOld; }
	CConfigNeedingRestart& GetConfigNew(void) { return m_ConfigNew; }
	bool IsConfigChanged(void) { return m_ConfigNew != m_ConfigOld; }
	void SetDoBenchmark(void) { m_bDoBenchmark = true; }

private:
	bool IsOkToSaveLoadState(HWND hWnd, const bool bConfigChanged);
	bool IsOkToRestart(HWND hWnd);
	void SaveComputerType(eApple2Type NewApple2Type);
	bool HardwareConfigChanged(HWND hWnd);
	bool CheckChangesForRestart(HWND hWnd);
	void ApplyNewConfig(void);
	void RestoreCurrentConfig(void);
	std::string GetSlot(const UINT uSlot);
	std::string GetCardName(const SS_CARDTYPE CardType);

	PAGETYPE m_LastPage;
	UINT32 m_bmPages;
	char m_szNewFilename[MAX_PATH];
	bool m_bSSNewFilename;
	char m_szSSNewDirectory[MAX_PATH];
	char m_szSSNewFilename[MAX_PATH];
	char m_szSSNewPathname[MAX_PATH];
	CConfigNeedingRestart m_ConfigOld;
	CConfigNeedingRestart m_ConfigNew;
	bool m_bDoBenchmark;
};
