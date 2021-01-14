#pragma once

#include "FrameBase.h"

class Video;

#if 0 // enable non-integral full-screen scaling
#define FULLSCREEN_SCALE_TYPE float
#else
#define FULLSCREEN_SCALE_TYPE int
#endif

class Win32Frame : public FrameBase
{
public:
	Win32Frame();

	static Win32Frame& GetWin32Frame();

	virtual void FrameDrawDiskLEDS();
	virtual void FrameDrawDiskStatus();

	virtual void FrameRefreshStatus(int drawflags);
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

	bool GetFullScreenShowSubunitStatus(void);
	int GetFullScreenOffsetX(void);
	int GetFullScreenOffsetY(void);
	bool IsFullScreen(void);
	void FrameRegisterClass();
	void FrameCreateWindow(void);
	void ChooseMonochromeColor(void);
	UINT Get3DBorderWidth(void);
	UINT Get3DBorderHeight(void);
	void ApplyVideoModeChange(void);
	LRESULT WndProc(HWND   window, UINT   message, WPARAM wparam, LPARAM lparam);

private:
	static BOOL CALLBACK DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext);

	void videoCreateDIBSection(Video& video);
	void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale);
	bool DDInit(void);
	void DDUninit(void);

	void Benchmark(void);
	void DisplayLogo(void);
	void FrameDrawDiskLEDS(HDC hdc);  // overloaded Win32 only, call via GetWin32Frame()
	void FrameDrawDiskStatus(HDC hdc);  // overloaded Win32 only, call via GetWin32Frame()
	void EraseButton(int number);
	void DrawButton(HDC passdc, int number);
	void DrawCrosshairs(int x, int y);
	void DrawFrameWindow(bool bPaintingWindow = false);
	void DrawStatusArea(HDC passdc, int drawflags);
	void ProcessButtonClick(int button, bool bFromButtonUI = false);
	bool ConfirmReboot(bool bFromButtonUI);
	void ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive);
	void RelayEvent(UINT message, WPARAM wparam, LPARAM lparam);
	void SetFullScreenMode();
	void SetNormalMode();
	void SetUsingCursor(BOOL bNewValue);
	void SetupTooltipControls(void);
	void FrameResizeWindow(int nNewScale);
	void RevealCursor();
	void ScreenWindowResize(const bool bCtrlKey);
	void UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x=0, int y=0);
	void DrawCrosshairsMouse();
	void FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen);
	void CreateGdiObjects(void);
	void FrameShowCursor(BOOL bShow);
	void FullScreenRevealCursor(void);

	bool g_bAltEnter_ToggleFullScreen; // Default for ALT+ENTER is to toggle between windowed and full-screen modes
	bool    g_bIsFullScreen;
	bool g_bShowingCursor;
	bool g_bLastCursorInAppleViewport;
	UINT_PTR	g_TimerIDEvent_100msec;
	UINT		g_uCount100msec;
	COLORREF      customcolors[256];	// MONOCHROME is last custom color
	HBITMAP       g_hLogoBitmap;
	HBITMAP       g_hDeviceBitmap;
	HDC           g_hDeviceDC;
	LPBITMAPINFO  g_pFramebufferinfo;
	BOOL    g_bUsingCursor;	// TRUE = AppleWin is using (hiding) the mouse-cursor && restricting cursor to window - see SetUsingCursor()
	bool    g_bAppActive;
	bool g_bFrameActive;
	bool g_windowMinimized;
	std::string driveTooltip;
	bool g_bFullScreen_ShowSubunitStatus;
	FULLSCREEN_SCALE_TYPE	g_win_fullscreen_scale;
	int						g_win_fullscreen_offsetx;
	int						g_win_fullscreen_offsety;

	static const UINT MAX_DRAW_DEVICES = 10;
	char* draw_devices[MAX_DRAW_DEVICES];
	GUID draw_device_guid[MAX_DRAW_DEVICES];
	int num_draw_devices;
	LPDIRECTDRAW g_lpDD;
};
