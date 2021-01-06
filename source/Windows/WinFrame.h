#pragma once

// Win32
	extern int        g_nViewportCX;
	extern int        g_nViewportCY;


// Emulator
	extern bool   g_bScrollLock_FullSpeed;


// Prototypes
	HDC     FrameGetDC ();
	void    FrameReleaseDC ();
	int		GetViewportScale(void);
	void	GetViewportCXCY(int& nViewportCX, int& nViewportCY);

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );
