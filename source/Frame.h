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
	extern BOOL       g_bIsFullScreen;
	extern int        g_nViewportCX;
	extern int        g_nViewportCY;
	extern BOOL       g_bConfirmReboot; // saved PageConfig REGSAVE
	extern BOOL       g_bMultiMon;


// Emulator
	extern bool   g_bFreshReset;
	extern std::string PathFilename[2];
	extern bool   g_bScrollLock_FullSpeed;
	extern int    g_nCharsetType;


#if 0 // enable non-integral full-screen scaling
#define FULLSCREEN_SCALE_TYPE float
#else
#define FULLSCREEN_SCALE_TYPE int
#endif
extern FULLSCREEN_SCALE_TYPE	g_win_fullscreen_scale;
extern int		g_win_fullscreen_offsetx;
extern int		g_win_fullscreen_offsety;


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
	int     SetViewportScale(int nNewScale);
	void	GetViewportCXCY(int& nViewportCX, int& nViewportCY);
	bool	GetFullScreen32Bit(void);
	void	SetFullScreen32Bit(bool b32Bit);
	void    FrameUpdateApple2Type(void);

	void	FrameDrawDiskLEDS( HDC hdc );
	void	FrameDrawDiskStatus( HDC hdc );

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );

