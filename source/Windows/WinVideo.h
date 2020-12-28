#pragma once

#include "Video.h"

class WinVideo : public Video
{
public:
	WinVideo()
	{
		g_pFramebufferinfo = NULL;
		num_draw_devices = 0;
		g_lpDD = NULL;
	}

	virtual ~WinVideo()
	{
	}

	virtual void Initialize(void);
	virtual void Destroy(void);

	virtual void VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit = false);
	virtual void VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame);
	virtual void VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode = 0, bool bRedrawWholeScreen = false);
	virtual void Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename);
	virtual void ChooseMonochromeColor(void);
	virtual void Benchmark(void);
	virtual void DisplayLogo(void);

private:
	void videoCreateDIBSection(void);
	void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale);
	static BOOL CALLBACK WinVideo::DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext);
	bool DDInit(void);
	void DDUninit(void);

	COLORREF      customcolors[256];	// MONOCHROME is last custom color
	HBITMAP       g_hLogoBitmap;
	HBITMAP       g_hDeviceBitmap;
	HDC           g_hDeviceDC;
	LPBITMAPINFO  g_pFramebufferinfo;

	static const UINT MAX_DRAW_DEVICES = 10;
	char *draw_devices[MAX_DRAW_DEVICES];
	GUID draw_device_guid[MAX_DRAW_DEVICES];
	int num_draw_devices;
	LPDIRECTDRAW g_lpDD;
};
