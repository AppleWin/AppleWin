#pragma once

// Win32
	extern int        g_nViewportCX;
	extern int        g_nViewportCY;


// Emulator
	extern bool   g_bScrollLock_FullSpeed;


// Prototypes
	void    FrameCreateWindow(void);
	HDC     FrameGetDC ();
	void    FrameReleaseDC ();
	void    FrameRegisterClass ();
	int		GetViewportScale(void);
	void	GetViewportCXCY(int& nViewportCX, int& nViewportCY);

	bool	IsFullScreen(void);
	bool	GetFullScreenShowSubunitStatus(void);

	LRESULT CALLBACK FrameWndProc (
		HWND   window,
		UINT   message,
		WPARAM wparam,
		LPARAM lparam );

	int GetFullScreenOffsetX(void);
	int GetFullScreenOffsetY(void);

	UINT Get3DBorderWidth(void);
	UINT Get3DBorderHeight(void);
