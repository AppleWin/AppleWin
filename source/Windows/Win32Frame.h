#pragma once

#include "FrameBase.h"

class Video;

class Win32Frame : public FrameBase
{
public:
	Win32Frame();

	virtual void FrameDrawDiskLEDS(HDC hdc);
	virtual void FrameDrawDiskStatus(HDC hdc);
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
	virtual void ChooseMonochromeColor(void);
	virtual void Benchmark(void);
	virtual void DisplayLogo(void);
private:
	void videoCreateDIBSection(Video& video);
	void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale);
	static BOOL CALLBACK DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext);
	bool DDInit(void);
	void DDUninit(void);

	uint8_t* g_pFramebufferbits;

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
