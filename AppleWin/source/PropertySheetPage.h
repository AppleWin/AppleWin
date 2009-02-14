#pragma once

// Globals
	extern UINT g_uScrollLockToggle;
	extern UINT g_uMouseShowCrosshair;
	extern UINT g_uMouseRestrictToWindow;
	extern UINT g_uTheFreezesF8Rom;
	extern DWORD g_uCloneType;
	extern HWND hwConfigTab;
	extern HWND hwAdvancedTab;



// Prototypes
	void    PSP_Init();
	DWORD   PSP_GetVolumeMax();
	bool    PSP_SaveStateSelectImage(HWND hWindow, bool bSave);
	void ui_tfe_settings_dialog(HWND hwnd);
	void * get_tfe_interface(void);
	void get_tfe_enabled(int *tfe_enabled);
	string BrowseToFile(HWND hWindow, TCHAR* pszTitle, TCHAR* REGVALUE, TCHAR* FILEMASKS);

	void Config_Save_Video();
	void Config_Load_Video();
