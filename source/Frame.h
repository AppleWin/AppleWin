#pragma once

enum {NOT_ASCII=0, ASCII};

extern HWND       g_hFrameWindow;
extern BOOL       fullscreen;

void    FrameCreateWindow ();
HDC     FrameGetDC ();
HDC     FrameGetVideoDC (LPBYTE *,LONG *);
void    FrameRefreshStatus (int);
void    FrameRegisterClass ();
void    FrameReleaseDC ();
void    FrameReleaseVideoDC ();
