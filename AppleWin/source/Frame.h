#pragma once

enum {NOT_ASCII=0, ASCII};

// Win32
extern HWND       g_hFrameWindow;

extern BOOL       fullscreen;

void    FrameCreateWindow ();
HDC     FrameGetDC ();
HDC     FrameGetVideoDC (LPBYTE *,LONG *);
void    FrameRefreshStatus (int);
void    FrameRegisterClass ();
void    FrameReleaseDC ();
void    FrameReleaseVideoDC ();

LRESULT CALLBACK FrameWndProc (
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam );
