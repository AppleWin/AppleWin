#pragma once

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

	// Keyboard -- keystroke type
	enum {NOT_ASCII=0, ASCII};

// 3D Border
	#define  VIEWPORTX   5
	#define  VIEWPORTY   5

	// 560 = Double Hi-Res
	// 384 = Doule Scan Line
	#define  FRAMEBUFFER_W  560
	#define  FRAMEBUFFER_H  384

// Direct Draw -- For Full Screen
	extern	LPDIRECTDRAW        g_pDD;
	extern	LPDIRECTDRAWSURFACE g_pDDPrimarySurface;
	extern	IDirectDrawPalette* g_pDDPal;

// Win32
	extern HWND       g_hFrameWindow;
	extern HDC        g_hFrameDC;
	extern BOOL       g_bIsFullScreen;
	extern BOOL       g_bMultiMon;

// Emulator
	extern bool   g_bFreshReset;
	extern string PathFilename[2];
	extern bool   g_bScrollLock_FullSpeed;
	extern int    g_nCharsetType;

// Prototypes
	void CtrlReset();

	void    FrameCreateWindow ();
	HDC     FrameGetDC ();
	HDC     FrameGetVideoDC (LPBYTE *,LONG *);
	void    FrameRefreshStatus (int);
	void    FrameRegisterClass ();
	void    FrameReleaseDC ();
	void    FrameReleaseVideoDC ();
	void	FrameSetCursorPosByMousePos();

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );

