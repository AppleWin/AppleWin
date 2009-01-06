#pragma once

void    PSP_Init();
DWORD   PSP_GetVolumeMax();
bool    PSP_SaveStateSelectImage(HWND hWindow, bool bSave);
void ui_tfe_settings_dialog(HWND hwnd);
void * get_tfe_interface(void);
void get_tfe_enabled(int *tfe_enabled);
string BrowseToCiderPress (HWND hWindow, TCHAR* pszTitle);

extern UINT g_uScrollLockToggle;
extern UINT g_uMouseInSlot4;
extern UINT g_uMouseShowCrosshair;
extern UINT g_uMouseRestrictToWindow;
extern UINT g_uTheFreezesF8Rom;
extern DWORD g_uCloneType;
extern HWND hwConfigTab;
extern HWND hwAdvancedTab;

extern UINT g_uZ80InSlot5;
