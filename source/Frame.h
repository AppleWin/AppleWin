#pragma once

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

void    FrameCreateWindow ();
HDC     FrameGetDC ();
HDC     FrameGetVideoDC (LPBYTE *,LONG *);
void    FrameRefreshStatus (int);
void    FrameRegisterClass ();
void    FrameReleaseDC ();
void    FrameReleaseVideoDC ();
void	FrameSetCursorPosByMousePos();

extern  string PathFilename[2];

LRESULT CALLBACK FrameWndProc (
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam );

extern bool g_bScrollLock_FullSpeed;
extern int g_nCharsetType;
