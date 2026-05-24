#pragma once

void SingleStep(bool bReinit);

//===========================================

// Win32
bool GetLoadedSaveStateFlag();
bool GetHookAltTab();
bool GetHookAltGrControl();
bool GetFullScreenResolutionChangedByUser();

extern bool g_bRestartFullScreen;
