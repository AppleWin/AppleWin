#pragma once

void SingleStep(bool bReinit);

//===========================================

// Win32
bool GetLoadedSaveStateFlag();
bool GetFullScreenResolutionChangedByUser();

extern bool g_bRestartFullScreen;
