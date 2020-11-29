#pragma once

extern HINSTANCE  g_hInstance;
extern HWND       g_hFrameWindow;
extern BOOL       g_bConfirmReboot; // saved PageConfig REGSAVE
extern BOOL       g_bMultiMon;

void	FrameDrawDiskLEDS(HDC hdc);
void	FrameDrawDiskStatus(HDC hdc);
void	FrameRefreshStatus(int, bool bUpdateDiskStatus = true);
void	FrameUpdateApple2Type();
void    FrameSetCursorPosByMousePos();

void	VideoRedrawScreen();
void	SetFullScreenShowSubunitStatus(bool bShow);
bool	GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0);
int		SetViewportScale(int nNewScale, bool bForce = false);
void	SetAltEnterToggleFullScreen(bool mode);

void	SetLoadedSaveStateFlag(const bool bFlag);
