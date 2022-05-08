#include "StdAfx.h"

#include "Windows/Win32Frame.h"
#include "Interface.h"
#include "Core.h"
#include "CPU.h"
#include "Joystick.h"
#include "Log.h"
#include "Memory.h"
#include "CardManager.h"
#include "Debugger/Debug.h"
#include "Tfe/PCapBackend.h"
#include "../resource/resource.h"

// Win32Frame methods are implemented in AppleWin, WinFrame and WinVideo.
// in time they should be brought together and more freestanding functions added to Win32Frame.

Win32Frame::Win32Frame()
{
	g_pFramebufferinfo = NULL;
	num_draw_devices = 0;
	g_lpDD = NULL;
	g_hLogoBitmap = (HBITMAP)0;
	g_hDeviceBitmap = (HBITMAP)0;
	g_hDeviceDC = (HDC)0;
	g_bAltEnter_ToggleFullScreen = false;
	g_bIsFullScreen = false;
	g_bShowingCursor = true;
	g_bLastCursorInAppleViewport = false;
	g_uCount100msec = 0;
	g_TimerIDEvent_100msec = 0;
	g_bUsingCursor = FALSE;
	g_bAppActive = false;
	g_bFrameActive = false;
	g_windowMinimized = false;
	g_bFullScreen_ShowSubunitStatus = true;
	g_win_fullscreen_offsetx = 0;
	g_win_fullscreen_offsety = 0;
	m_bestWidthForFullScreen = 0;
	m_bestHeightForFullScreen = 0;
	m_changedDisplaySettings = false;

	g_nMaxViewportScale = kDEFAULT_VIEWPORT_SCALE;	// Max scale in Windowed mode with borders, buttons etc (full-screen may be +1)

	btnfacebrush = (HBRUSH)0;
	btnfacepen = (HPEN)0;
	btnhighlightpen = (HPEN)0;
	btnshadowpen = (HPEN)0;
	buttonactive = -1;
	buttondown = -1;
	buttonover = -1;
	g_hFrameDC = (HDC)0;
	memset(&framerect, 0, sizeof(framerect));

	helpquit = 0;
	smallfont = (HFONT)0;
	tooltipwindow = (HWND)0;
	viewportx = VIEWPORTX;	// Default to Normal (non-FullScreen) mode
	viewporty = VIEWPORTY;	// Default to Normal (non-FullScreen) mode

	g_bScrollLock_FullSpeed = false;

	g_nTrackDrive1 = -1;
	g_nTrackDrive2 = -1;
	g_nSectorDrive1 = -1;
	g_nSectorDrive2 = -1;
	g_strTrackDrive1 = "??";
	g_strTrackDrive2 = "??";
	g_strSectorDrive1 = "??";
	g_strSectorDrive2 = "??";

	g_eStatusDrive1 = DISK_STATUS_OFF;
	g_eStatusDrive2 = DISK_STATUS_OFF;

	// Set g_nViewportScale, g_nViewportCX, g_nViewportCY & buttonx, buttony
	SetViewportScale(kDEFAULT_VIEWPORT_SCALE, true);
}

void Win32Frame::VideoCreateDIBSection(bool resetVideoState)
{
	// CREATE THE DEVICE CONTEXT
	HWND window = GetDesktopWindow();
	HDC dc = GetDC(window);
	if (g_hDeviceDC)
	{
		DeleteDC(g_hDeviceDC);
	}
	g_hDeviceDC = CreateCompatibleDC(dc);

	// CREATE THE FRAME BUFFER DIB SECTION
	if (g_hDeviceBitmap)
	{
		DeleteObject(g_hDeviceBitmap);
		GetVideo().Destroy();
	}

	uint8_t* pFramebufferbits;

	g_hDeviceBitmap = CreateDIBSection(
		dc,
		g_pFramebufferinfo,
		DIB_RGB_COLORS,
		(LPVOID*)&pFramebufferbits, 0, 0
	);
	SelectObject(g_hDeviceDC, g_hDeviceBitmap);
	GetVideo().Initialize(pFramebufferbits, resetVideoState);
}

void Win32Frame::Initialize(bool resetVideoState)
{
	if (g_hLogoBitmap == NULL)
	{
		// LOAD THE LOGO
		g_hLogoBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN));
	}

	if (g_pFramebufferinfo)
		delete[] g_pFramebufferinfo;

	// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
	g_pFramebufferinfo = (LPBITMAPINFO) new BYTE[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];

	memset(g_pFramebufferinfo, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	g_pFramebufferinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_pFramebufferinfo->bmiHeader.biWidth = GetVideo().GetFrameBufferWidth();
	g_pFramebufferinfo->bmiHeader.biHeight = GetVideo().GetFrameBufferHeight();
	g_pFramebufferinfo->bmiHeader.biPlanes = 1;
	g_pFramebufferinfo->bmiHeader.biBitCount = 32;
	g_pFramebufferinfo->bmiHeader.biCompression = BI_RGB;
	g_pFramebufferinfo->bmiHeader.biClrUsed = 0;

	VideoCreateDIBSection(resetVideoState);

#if 0
	DDInit();	// For WaitForVerticalBlank()
#endif
}

void Win32Frame::Destroy(void)
{
	// DESTROY BUFFERS
	delete[] g_pFramebufferinfo;
	g_pFramebufferinfo = NULL;

	// DESTROY FRAME BUFFER
	DeleteDC(g_hDeviceDC);
	g_hDeviceDC = (HDC)0;

	DeleteObject(g_hDeviceBitmap);	// this invalidates the Video's FrameBuffer pointer
	GetVideo().Destroy(); // this resets the Video's FrameBuffer pointer
	g_hDeviceBitmap = (HBITMAP)0;

	// DESTROY LOGO
	if (g_hLogoBitmap) {
		DeleteObject(g_hLogoBitmap);
		g_hLogoBitmap = (HBITMAP)0;
	}

	DDUninit();
}

//===========================================================================
void Win32Frame::Benchmark(void)
{
	_ASSERT(g_nAppMode == MODE_BENCHMARK);
	Sleep(500);
	Video& video = GetVideo();

	// PREPARE TWO DIFFERENT FRAME BUFFERS, EACH OF WHICH HAVE HALF OF THE
	// BYTES SET TO 0x14 AND THE OTHER HALF SET TO 0xAA
	int     loop;
	LPDWORD mem32 = (LPDWORD)mem;
	for (loop = 4096; loop < 6144; loop++)
		*(mem32 + loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0x14141414
		: 0xAAAAAAAA;
	for (loop = 6144; loop < 8192; loop++)
		*(mem32 + loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0xAAAAAAAA
		: 0x14141414;

	// SEE HOW MANY TEXT FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
	// GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
	// SIMULATE THE ACTIVITY OF AN AVERAGE GAME
	DWORD totaltextfps = 0;

	video.SetVideoMode(VF_TEXT);
	memset(mem + 0x400, 0x14, 0x400);
	VideoRedrawScreen();
	DWORD milliseconds = GetTickCount();
	while (GetTickCount() == milliseconds);
	milliseconds = GetTickCount();
	DWORD cycle = 0;
	do {
		if (cycle & 1)
			memset(mem + 0x400, 0x14, 0x400);
		else
			memcpy(mem + 0x400, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x400);
		VideoPresentScreen();
		if (cycle++ >= 3)
			cycle = 0;
		totaltextfps++;
	} while (GetTickCount() - milliseconds < 1000);

	// SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
	// GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
	// SIMULATE THE ACTIVITY OF AN AVERAGE GAME
	DWORD totalhiresfps = 0;
	video.SetVideoMode(VF_HIRES);
	memset(mem + 0x2000, 0x14, 0x2000);
	VideoRedrawScreen();
	milliseconds = GetTickCount();
	while (GetTickCount() == milliseconds);
	milliseconds = GetTickCount();
	cycle = 0;
	do {
		if (cycle & 1)
			memset(mem + 0x2000, 0x14, 0x2000);
		else
			memcpy(mem + 0x2000, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x2000);
		VideoPresentScreen();
		if (cycle++ >= 3)
			cycle = 0;
		totalhiresfps++;
	} while (GetTickCount() - milliseconds < 1000);

	// DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
	// NOTHING ELSE GOING ON
	DWORD totalmhz10[2] = { 0,0 };	// bVideoUpdate & !bVideoUpdate
	for (UINT i = 0; i < 2; i++)
	{
		CpuSetupBenchmark();
		milliseconds = GetTickCount();
		while (GetTickCount() == milliseconds);
		milliseconds = GetTickCount();
		do {
			CpuExecute(100000, i == 0 ? true : false);
			totalmhz10[i]++;
		} while (GetTickCount() - milliseconds < 1000);
	}

	// IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
	// CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
	if ((regs.pc < 0x300) || (regs.pc > 0x400))
		if (FrameMessageBox(
			TEXT("The emulator has detected a problem while running ")
			TEXT("the CPU benchmark.  Would you like to gather more ")
			TEXT("information?"),
			TEXT("Benchmarks"),
			MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES) {
			BOOL error = 0;
			WORD lastpc = 0x300;
			int  loop = 0;
			while ((loop < 10000) && !error) {
				CpuSetupBenchmark();
				CpuExecute(loop, true);
				if ((regs.pc < 0x300) || (regs.pc > 0x400))
					error = 1;
				else {
					lastpc = regs.pc;
					++loop;
				}
			}
			if (error) {
				std::string strText = StrFormat(
					"The emulator experienced an error %u clock cycles "
					"into the CPU benchmark.  Prior to the error, the "
					"program counter was at $%04X.  After the error, it "
					"had jumped to $%04X.",
					(unsigned)loop,
					(unsigned)lastpc,
					(unsigned)regs.pc);
				FrameMessageBox(
					strText.c_str(),
					"Benchmarks",
					MB_ICONINFORMATION | MB_SETFOREGROUND);
			}
			else
				FrameMessageBox(
					"The emulator was unable to locate the exact "
					"point of the error.  This probably means that "
					"the problem is external to the emulator, "
					"happening asynchronously, such as a problem in "
					"a timer interrupt handler.",
					"Benchmarks",
					MB_ICONINFORMATION | MB_SETFOREGROUND);
		}

	// DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
	// WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
	// THE SAME TIME
	DWORD realisticfps = 0;
	memset(mem + 0x2000, 0xAA, 0x2000);
	VideoRedrawScreen();
	milliseconds = GetTickCount();
	while (GetTickCount() == milliseconds);
	milliseconds = GetTickCount();
	cycle = 0;
	do {
		if (realisticfps < 10) {
			int cycles = 100000;
			while (cycles > 0) {
				DWORD executedcycles = CpuExecute(103, true);
				cycles -= executedcycles;
				GetCardMgr().GetDisk2CardMgr().Update(executedcycles);
			}
		}
		if (cycle & 1)
			memset(mem + 0x2000, 0xAA, 0x2000);
		else
			memcpy(mem + 0x2000, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x2000);
		VideoRedrawScreen();
		if (cycle++ >= 3)
			cycle = 0;
		realisticfps++;
	} while (GetTickCount() - milliseconds < 1000);

	// DISPLAY THE RESULTS
	DisplayLogo();
	std::string strText = StrFormat(
		"Pure Video FPS:\t%u hires, %u text\n"
		"Pure CPU MHz:\t%u.%u%s (video update)\n"
		"Pure CPU MHz:\t%u.%u%s (full-speed)\n\n"
		"EXPECTED AVERAGE VIDEO GAME\n"
		"PERFORMANCE: %u FPS",
		(unsigned)totalhiresfps,
		(unsigned)totaltextfps,
		(unsigned)(totalmhz10[0] / 10), (unsigned)(totalmhz10[0] % 10), (LPCTSTR)(IS_APPLE2 ? " (6502)" : ""),
		(unsigned)(totalmhz10[1] / 10), (unsigned)(totalmhz10[1] % 10), (LPCTSTR)(IS_APPLE2 ? " (6502)" : ""),
		(unsigned)realisticfps);
	FrameMessageBox(
		strText.c_str(),
		"Benchmarks",
		MB_ICONINFORMATION | MB_SETFOREGROUND);
}

//===========================================================================

// This is called from PageConfig
void Win32Frame::ChooseMonochromeColor(void)
{
	Video& video = GetVideo();
	CHOOSECOLOR cc;
	memset(&cc, 0, sizeof(CHOOSECOLOR));
	cc.lStructSize = sizeof(CHOOSECOLOR);
	cc.hwndOwner = g_hFrameWindow;
	cc.rgbResult = video.GetMonochromeRGB();
	cc.lpCustColors = customcolors + 1;
	cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;
	if (ChooseColor(&cc))
	{
		video.SetMonochromeRGB(cc.rgbResult);
		ApplyVideoModeChange();
	}
}

//===========================================================================

void Win32Frame::VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale)
{
	HDC hSrcDC = CreateCompatibleDC(hDstDC);
	SelectObject(hSrcDC, g_hLogoBitmap);
	StretchBlt(
		hDstDC,   // hdcDest
		xoff, yoff,  // nXDest, nYDest
		scale * srcw, scale * srch, // nWidth, nHeight
		hSrcDC,   // hdcSrc
		0, 0,     // nXSrc, nYSrc
		srcw, srch,
		SRCCOPY   // dwRop
	);

	DeleteObject(hSrcDC);
}

//===========================================================================

void Win32Frame::DisplayLogo(void)
{
	Video& video = GetVideo();
	int nLogoX = 0, nLogoY = 0;
	int scale = GetViewportScale();

	HDC hFrameDC = FrameGetDC();

	// DRAW THE LOGO
	SelectObject(hFrameDC, GetStockObject(NULL_PEN));

	if (g_hLogoBitmap)
	{
		BITMAP bm;
		if (GetObject(g_hLogoBitmap, sizeof(bm), &bm))
		{
			nLogoX = (g_nViewportCX - scale * bm.bmWidth) / 2;
			nLogoY = (g_nViewportCY - scale * bm.bmHeight) / 2;

			if (IsFullScreen())
			{
				nLogoX += GetFullScreenOffsetX();
				nLogoY += GetFullScreenOffsetY();
			}

			VideoDrawLogoBitmap(hFrameDC, nLogoX, nLogoY, bm.bmWidth, bm.bmHeight, scale);
		}
	}

	// DRAW THE VERSION NUMBER
	TCHAR sFontName[] = TEXT("Arial");
	HFONT font = CreateFont(-20, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		VARIABLE_PITCH | 4 | FF_SWISS,
		sFontName);
	SelectObject(hFrameDC, font);
	SetTextAlign(hFrameDC, TA_RIGHT | TA_TOP);
	SetBkMode(hFrameDC, TRANSPARENT);

	std::string strVersion = "Version " + g_VERSIONSTRING;
	int xoff = GetFullScreenOffsetX(), yoff = GetFullScreenOffsetY();

#define  DRAWVERSION(x,y,c)                 \
	SetTextColor(hFrameDC,c);               \
	TextOut(hFrameDC,                       \
		scale*540+x+xoff,scale*358+y+yoff,  \
		strVersion.c_str(),                 \
		strVersion.length());

	if (GetDeviceCaps(hFrameDC, PLANES) * GetDeviceCaps(hFrameDC, BITSPIXEL) <= 4) {
		DRAWVERSION(2, 2, RGB(0x00, 0x00, 0x00));
		DRAWVERSION(1, 1, RGB(0x00, 0x00, 0x00));
		DRAWVERSION(0, 0, RGB(0xFF, 0x00, 0xFF));
	}
	else {
		DRAWVERSION(1, 1, PALETTERGB(0x30, 0x30, 0x70));
		DRAWVERSION(-1, -1, PALETTERGB(0xC0, 0x70, 0xE0));
		DRAWVERSION(0, 0, PALETTERGB(0x70, 0x30, 0xE0));
	}

#if _DEBUG
	strVersion = "DEBUG";
	DRAWVERSION(2, -358 * scale, RGB(0x00, 0x00, 0x00));
	DRAWVERSION(1, -357 * scale, RGB(0x00, 0x00, 0x00));
	DRAWVERSION(0, -356 * scale, RGB(0xFF, 0x00, 0xFF));
#endif

#undef  DRAWVERSION

	DeleteObject(font);
}

//===========================================================================

void Win32Frame::VideoPresentScreen(void)
{
	HDC hFrameDC = FrameGetDC();

	if (hFrameDC)
	{
		Video& video = GetVideo();
		int xSrc = video.GetFrameBufferBorderWidth();
		int ySrc = video.GetFrameBufferBorderHeight();

		int xdest = IsFullScreen() ? GetFullScreenOffsetX() : 0;
		int ydest = IsFullScreen() ? GetFullScreenOffsetY() : 0;
		int wdest = g_nViewportCX;
		int hdest = g_nViewportCY;

		SetStretchBltMode(hFrameDC, COLORONCOLOR);
		StretchBlt(
			hFrameDC,
			xdest, ydest,
			wdest, hdest,
			g_hDeviceDC,
			xSrc, ySrc,
			video.GetFrameBufferBorderlessWidth(), video.GetFrameBufferBorderlessHeight(),
			SRCCOPY);
	}

#ifdef NO_DIRECT_X
#else
	//if (g_lpDD) g_lpDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
#endif // NO_DIRECT_X

	GdiFlush();
}

//===========================================================================

BOOL CALLBACK Win32Frame::DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
	Win32Frame* obj = (Win32Frame*)lpContext;

	int i = obj->num_draw_devices;
	if (i == MAX_DRAW_DEVICES)
		return TRUE;
	if (lpGUID != NULL)
		memcpy(&(obj->draw_device_guid[i]), lpGUID, sizeof(GUID));
	obj->draw_devices[i] = _strdup(lpszDesc);

	if (g_fh) fprintf(g_fh, "%d: %s - %s\n", i, lpszDesc, lpszDrvName);

	(obj->num_draw_devices)++;
	return TRUE;
}

bool Win32Frame::DDInit(void)
{
#ifdef NO_DIRECT_X

	return false;

#else
	HRESULT hr = DirectDrawEnumerate((LPDDENUMCALLBACK)DDEnumProc, this);
	if (FAILED(hr))
	{
		LogFileOutput("DSEnumerate failed (%08X)\n", hr);
		return false;
	}

	LogFileOutput("Number of draw devices = %d\n", num_draw_devices);

	bool bCreatedOK = false;
	for (int x = 0; x < num_draw_devices; x++)
	{
		hr = DirectDrawCreate(&draw_device_guid[x], &g_lpDD, NULL);
		if (SUCCEEDED(hr))
		{
			LogFileOutput("DSCreate succeeded for draw device #%d\n", x);
			bCreatedOK = true;
			break;
		}

		LogFileOutput("DSCreate failed for draw device #%d (%08X)\n", x, hr);
	}

	if (!bCreatedOK)
	{
		LogFileOutput("DSCreate failed for all draw devices\n");
		return false;
	}

	return true;
#endif // NO_DIRECT_X
}

// From SoundCore.h
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

void Win32Frame::DDUninit(void)
{
	SAFE_RELEASE(g_lpDD);
}

#undef SAFE_RELEASE

void Win32Frame::ApplyVideoModeChange(void)
{
	Video& video = GetVideo();
	video.Config_Save_Video();
	video.VideoReinitialize(false);

	if (g_nAppMode != MODE_LOGO)
	{
		if (g_nAppMode == MODE_DEBUG)
		{
			UINT debugVideoMode;
			if (DebugGetVideoMode(&debugVideoMode))
				VideoRefreshScreen(debugVideoMode, true);
			else
				VideoPresentScreen();
		}
		else
		{
			VideoPresentScreen();
		}
	}
}

Win32Frame& Win32Frame::GetWin32Frame()
{
	FrameBase& frameBase = GetFrame();
	Win32Frame& win32Frame = dynamic_cast<Win32Frame&>(frameBase);
	return win32Frame;
}

int Win32Frame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	const HWND handle = g_hFrameWindow ? g_hFrameWindow : GetDesktopWindow();
	return MessageBox(handle, lpText, lpCaption, uType);
}

void Win32Frame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
{
	HBITMAP hBitmap = LoadBitmap(g_hInstance, lpBitmapName);
	GetBitmapBits(hBitmap, cb, lpvBits);
	DeleteObject(hBitmap);
}

void Win32Frame::Restart()
{
	// Changed h/w config, eg. Apple computer type (][+ or //e), slot configuration, etc.
	g_bRestart = true;
	PostMessage(g_hFrameWindow, WM_CLOSE, 0, 0);
}

BYTE* Win32Frame::GetResource(WORD id, LPCSTR lpType, DWORD dwExpectedSize)
{
	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(id), lpType);
	if (hResInfo == NULL)
		return NULL;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if (dwResSize != dwExpectedSize)
		return NULL;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if (hResData == NULL)
		return NULL;

	BYTE* pResource = (BYTE*)LockResource(hResData);	// NB. Don't need to unlock resource

	return pResource;
}

std::string Win32Frame::Video_GetScreenShotFolder() const
{
	// save in current folder
	return std::string();
}

std::shared_ptr<NetworkBackend> Win32Frame::CreateNetworkBackend(const std::string & interfaceName)
{
	std::shared_ptr<NetworkBackend> backend(new PCapBackend(interfaceName));
	return backend;
}
