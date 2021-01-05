#pragma once

#include "FrameBase.h"

class Video;

class Win32Frame : public FrameBase
{
public:
	Win32Frame();

	void FrameDrawDiskLEDS(HDC hdc);  // overloaded Win32 only, call via GetWin32Frame()
	virtual void FrameDrawDiskLEDS();

	void FrameDrawDiskStatus(HDC hdc);  // overloaded Win32 only, call via GetWin32Frame()
	virtual void FrameDrawDiskStatus();

	virtual void FrameRefreshStatus(int, bool bUpdateDiskStatus = true);
	virtual void FrameUpdateApple2Type();
	virtual void FrameSetCursorPosByMousePos();

	virtual void SetFullScreenShowSubunitStatus(bool bShow);
	virtual bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0);
	virtual int SetViewportScale(int nNewScale, bool bForce = false);
	virtual void SetAltEnterToggleFullScreen(bool mode);

	virtual void SetLoadedSaveStateFlag(const bool bFlag);

	virtual void Initialize(void);
	virtual void Destroy(void);
	virtual void VideoPresentScreen(void);
			void ChooseMonochromeColor(void);
			void Benchmark(void);
			void DisplayLogo(void);

	static Win32Frame& GetWin32Frame();

private:
	void videoCreateDIBSection(Video& video);
	void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale);
	static BOOL CALLBACK DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext);
	bool DDInit(void);
	void DDUninit(void);

	COLORREF      customcolors[256];	// MONOCHROME is last custom color
	HBITMAP       g_hLogoBitmap;
	HBITMAP       g_hDeviceBitmap;
	HDC           g_hDeviceDC;
	LPBITMAPINFO  g_pFramebufferinfo;

	static const UINT MAX_DRAW_DEVICES = 10;
	char* draw_devices[MAX_DRAW_DEVICES];
	GUID draw_device_guid[MAX_DRAW_DEVICES];
	int num_draw_devices;
	LPDIRECTDRAW g_lpDD;
};
