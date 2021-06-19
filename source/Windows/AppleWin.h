#pragma once

void SingleStep(bool bReinit);

//===========================================

// Win32
bool GetLoadedSaveStateFlag(void);
bool GetHookAltTab(void);
bool GetHookAltGrControl(void);

extern bool g_bRestartFullScreen;
