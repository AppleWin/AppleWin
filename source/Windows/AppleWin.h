#pragma once

void SingleStep(bool bReinit);

//===========================================

// Win32
bool GetLoadedSaveStateFlag(void);
bool GetHookAltTab(void);
bool GetHookAltGrControl(void);
bool GetFullScreenResolutionChangedByUser(void);

extern bool g_bRestartFullScreen;
