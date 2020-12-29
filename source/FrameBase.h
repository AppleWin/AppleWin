#pragma once

class FrameBase
{
public:
	FrameBase();

	virtual ~FrameBase();

	HINSTANCE  g_hInstance;
	HWND       g_hFrameWindow;
	BOOL       g_bConfirmReboot; // saved PageConfig REGSAVE
	BOOL       g_bMultiMon;
	bool       g_bFreshReset;

	void VideoRedrawScreen();

	virtual void FrameDrawDiskLEDS(HDC hdc) = 0;
	virtual void FrameDrawDiskStatus(HDC hdc) = 0;
	virtual void FrameRefreshStatus(int, bool bUpdateDiskStatus = true) = 0;
	virtual void FrameUpdateApple2Type() = 0;
	virtual void FrameSetCursorPosByMousePos() = 0;

	virtual void SetFullScreenShowSubunitStatus(bool bShow) = 0;
	virtual bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0) = 0;
	virtual int SetViewportScale(int nNewScale, bool bForce = false) = 0;
	virtual void SetAltEnterToggleFullScreen(bool mode) = 0;

	virtual void SetLoadedSaveStateFlag(const bool bFlag) = 0;
};
