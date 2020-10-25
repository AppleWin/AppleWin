#pragma once

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

#define FRAMEBUFFER_W 560
#define FRAMEBUFFER_H 384

// Win32
	extern HWND       g_hFrameWindow;
	extern int        g_nViewportCX;
	extern int        g_nViewportCY;
	extern BOOL       g_bConfirmReboot; // saved PageConfig REGSAVE
	extern BOOL       g_bMultiMon;

// Emulator
	extern bool   g_bFreshReset;
	extern std::string PathFilename[2];
	extern bool   g_bScrollLock_FullSpeed;

// Prototypes
	void CtrlReset();

	void    FrameCreateWindow(void);
	HDC     FrameGetDC ();
	void    FrameReleaseDC ();
	void    FrameRefreshStatus (int, bool bUpdateDiskStatus = true );
	void    FrameRegisterClass ();
	void	FrameSetCursorPosByMousePos();
	bool    GetDebugMode(void);
	void	SetDebugMode(bool newDebugMode);
	int		GetViewportScale(void);
	int     SetViewportScale(int nNewScale, bool bForce = false);
	void	GetViewportCXCY(int& nViewportCX, int& nViewportCY);
	void    FrameUpdateApple2Type(void);
	bool	GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight=0);

	bool	IsFullScreen(void);
	bool	GetFullScreenShowSubunitStatus(void);
	void	SetFullScreenShowSubunitStatus(bool bShow);

	void	FrameDrawDiskLEDS( HDC hdc );
	void	FrameDrawDiskStatus( HDC hdc );

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );

	int GetFullScreenOffsetX(void);
	int GetFullScreenOffsetY(void);

	UINT GetFrameBufferBorderlessWidth(void);
	UINT GetFrameBufferBorderlessHeight(void);
	UINT GetFrameBufferBorderWidth(void);
	UINT GetFrameBufferBorderHeight(void);
	UINT GetFrameBufferWidth(void);
	UINT GetFrameBufferHeight(void);
	UINT Get3DBorderWidth(void);
	UINT Get3DBorderHeight(void);

	void SetAltEnterToggleFullScreen(bool mode);
