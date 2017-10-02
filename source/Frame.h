#pragma once

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

	// Keyboard -- keystroke type
	enum {NOT_ASCII=0, ASCII};

// 3D Border
	#define  VIEWPORTX   5
	#define  VIEWPORTY   5

	// 560 = Double Hi-Res
	// 384 = Double Scan Line
	#define  FRAMEBUFFER_BORDERLESS_W 560
	#define  FRAMEBUFFER_BORDERLESS_H 384
// NTSC_BEGIN
#if 0
	// TC: No good as NTSC render code writes to border area:
	// . NTSC.cpp: updateVideoScannerHorzEOL(): "NOTE: This writes out-of-bounds for a 560x384 framebuffer"
	#define  BORDER_W       0
	#define  BORDER_H       0
	#define  FRAMEBUFFER_W  FRAMEBUFFER_BORDERLESS_W
	#define  FRAMEBUFFER_H  FRAMEBUFFER_BORDERLESS_H
#else
	#define  BORDER_W       20
	#define  BORDER_H       18
	#define  FRAMEBUFFER_W  (FRAMEBUFFER_BORDERLESS_W + BORDER_W*2)
	#define  FRAMEBUFFER_H  (FRAMEBUFFER_BORDERLESS_H + BORDER_H*2)
#endif
// NTSC_END

// Direct Draw -- For Full Screen
	extern	LPDIRECTDRAW        g_pDD;
	extern	LPDIRECTDRAWSURFACE g_pDDPrimarySurface;
	extern  int                 g_nDDFullScreenW;
	extern  int                 g_nDDFullScreenH;

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
	HDC     FrameGetVideoDC (LPBYTE *,LONG *);
	void    FrameRefreshStatus (int, bool bUpdateDiskStatus = true );
	void    FrameRegisterClass ();
	void    FrameReleaseDC ();
	void    FrameReleaseVideoDC ();
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
