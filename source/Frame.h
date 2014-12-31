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
// NTSC_BEGIN
#if 0
	#define  FRAMEBUFFER_W  560
	#define  FRAMEBUFFER_H  384
#else
	#define  FRAMEBUFFER_W  600
	#define  FRAMEBUFFER_H  420
#endif
// NTSC_END

// Direct Draw -- For Full Screen
	extern	LPDIRECTDRAW        g_pDD;
	extern	LPDIRECTDRAWSURFACE g_pDDPrimarySurface;
	extern	IDirectDrawPalette* g_pDDPal;

// Win32
	extern HWND       g_hFrameWindow;
	extern BOOL       g_bIsFullScreen;
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
	int     SetViewportScale(int nNewScale);
	void	GetViewportCXCY(int& nViewportCX, int& nViewportCY);
	bool	GetFullScreen32Bit(void);
	void	SetFullScreen32Bit(bool b32Bit);

	void	FrameDrawDiskLEDS( HDC hdc );
	void	FrameDrawDiskStatus( HDC hdc );

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );

