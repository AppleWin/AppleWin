#pragma once

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

// GH#555: Extend the 14M video modes by 1 pixel
// . 14M (DHGR,DGR,80COL) are shifted right by 1 pixel, so zero out the left-most visible pixel.
// .  7M (all other modes) are not shift right by 1 pixel, so zero out the right-most visible pixel.
// NB. This 1 pixel shift is a workaround for the 14M video modes actually start 7x 14M pixels to the left on *real h/w*.
// . 7x 14M pixels early + 1x 14M pixel shifted right = 2 complete color phase rotations.
// . ie. the 14M colors are correct, but being 1 pixel out is the closest we can get the 7M and 14M video modes to overlap.
// . The alternative is to render the 14M correct 7 pixels early, but have 7-pixel borders left (for 7M modes) or right (for 14M modes).
#define EXTEND_14M_VIDEO_BY_1_PIXEL 1
#if EXTEND_14M_VIDEO_BY_1_PIXEL
#define kFrameBufferBorderlessW 561
#else
#define kFrameBufferBorderlessW 560
#endif


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
	extern int    g_nCharsetType;


// Prototypes
	void CtrlReset();

	void    FrameCreateWindow(void);
	HDC     FrameGetDC ();
	void    FrameReleaseDC ();
	void    FrameRefreshStatus (int, bool bUpdateDiskStatus = true );
	void    FrameRegisterClass ();
	void	FrameSetCursorPosByMousePos();
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
