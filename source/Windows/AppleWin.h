#pragma once

void SingleStep(bool bReinit);

//===========================================

// Win32
extern HINSTANCE  g_hInstance;

bool GetLoadedSaveStateFlag(void);
void SetLoadedSaveStateFlag(const bool bFlag);
bool GetHookAltGrControl(void);

extern bool       g_bRestartFullScreen;
