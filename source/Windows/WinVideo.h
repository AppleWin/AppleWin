#pragma once

// Prototypes _______________________________________________________

void WinVideoInitialize();
void WinVideoDestroy();

void    VideoBenchmark ();
void    VideoChooseMonochromeColor (); // FIXME: Should be moved to PageConfig and call VideoSetMonochromeColor()
void    VideoDisplayLogo ();
void    VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit = false);
void    VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame);
void    VideoRedrawScreen (void);
void    VideoRefreshScreen (uint32_t uRedrawWholeScreenVideoMode = 0, bool bRedrawWholeScreen = false);

void Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename);

bool DDInit(void);
void DDUninit(void);
