#pragma once

void    PSP_Init();
DWORD   PSP_GetVolumeMax();
bool    PSP_SaveStateSelectImage(HWND hWindow, bool bSave);
void ui_tfe_settings_dialog(HWND hwnd);
void * get_tfe_interface(void);
void get_tfe_enabled(int *tfe_enabled);
