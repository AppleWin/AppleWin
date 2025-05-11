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
	std::string BrowseToFile(HWND hWindow, const char* pszTitle, const char* REGVALUE, const char* FILEMASKS);
	void SaveStateUpdate();
	int SaveStateSelectImage(HWND hWindow, const char* pszTitle, bool bSave);
	void PostMsgAfterClose(HWND hWnd, PAGETYPE page);

	void ResetPageMask(void) { m_bmPages = 0; }	// Req'd because cancelling doesn't clear the page-mask
	PAGETYPE GetLastPage(void) { return m_LastPage; }
	void SetLastPage(PAGETYPE page)
	{
		m_LastPage = page;
		m_bmPages |= 1<<(UINT32)page;
	}

	void SaveCurrentConfig(void);
	const std::string & GetSSNewFilename(void) { return m_szSSNewFilename; }
	const CConfigNeedingRestart& GetConfigOld(void) { return m_ConfigOld; }
	CConfigNeedingRestart& GetConfigNew(void) { return m_ConfigNew; }
	bool IsConfigChanged(void) { return m_ConfigNew != m_ConfigOld; }
	void SetDoBenchmark(void) { m_bDoBenchmark = true; }
	void ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld);
	void ApplyNewConfigFromSnapshot(const CConfigNeedingRestart& ConfigNew);
	void ConfigSaveApple2Type(eApple2Type apple2Type);
	void SetSlot(UINT slot, SS_CARDTYPE newCardType);

private:
	bool IsOkToSaveLoadState(HWND hWnd, const bool bConfigChanged);
	bool IsOkToRestart(HWND hWnd);
	void SaveComputerType(eApple2Type NewApple2Type);
	void SaveCpuType(eCpuType NewCpuType);
	bool HardwareConfigChanged(HWND hWnd);
	bool CheckChangesForRestart(HWND hWnd);
	void ApplyNewConfig(void);
	void RestoreCurrentConfig(void);
	std::string GetSlot(const UINT uSlot);

	PAGETYPE m_LastPage;
	UINT32 m_bmPages;
	bool m_bSSNewFilename;
	std::string m_szSSNewDirectory;
	std::string m_szSSNewFilename;
	CConfigNeedingRestart m_ConfigOld;
	CConfigNeedingRestart m_ConfigNew;
	bool m_bDoBenchmark;
};
