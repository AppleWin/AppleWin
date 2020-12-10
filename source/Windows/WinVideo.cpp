/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Emulation of video modes
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Windows/WinVideo.h"
#include "Windows/WinFrame.h"
#include "Windows/AppleWin.h"
#include "Video.h"
#include "Core.h"
#include "CPU.h"
#include "Joystick.h"
#include "Frame.h"
#include "Log.h"
#include "Memory.h"
#include "CardManager.h"
#include "NTSC.h"

#include "../resource/resource.h"

static COLORREF      customcolors[256];	// MONOCHROME is last custom color
static HBITMAP       g_hLogoBitmap;
static HBITMAP       g_hDeviceBitmap;
static HDC           g_hDeviceDC;
static LPBITMAPINFO  g_pFramebufferinfo = NULL;

static void videoCreateDIBSection()
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
		DeleteObject(g_hDeviceBitmap);

	g_hDeviceBitmap = CreateDIBSection(
		dc,
		g_pFramebufferinfo,
		DIB_RGB_COLORS,
		(LPVOID*)&g_pFramebufferbits, 0, 0
	);
	SelectObject(g_hDeviceDC, g_hDeviceBitmap);

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	memset(g_pFramebufferbits, 0, GetFrameBufferWidth() * GetFrameBufferHeight() * sizeof(bgra_t));

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	NTSC_VideoInit(g_pFramebufferbits);
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

void WinVideoInitialize()
{
	// RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
	VideoResetState();

	// LOAD THE LOGO
	g_hLogoBitmap = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN));

	// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
	g_pFramebufferinfo = (LPBITMAPINFO) new BYTE[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];

	memset(g_pFramebufferinfo, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	g_pFramebufferinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_pFramebufferinfo->bmiHeader.biWidth = GetFrameBufferWidth();
	g_pFramebufferinfo->bmiHeader.biHeight = GetFrameBufferHeight();
	g_pFramebufferinfo->bmiHeader.biPlanes = 1;
	g_pFramebufferinfo->bmiHeader.biBitCount = 32;
	g_pFramebufferinfo->bmiHeader.biCompression = BI_RGB;
	g_pFramebufferinfo->bmiHeader.biClrUsed = 0;

	videoCreateDIBSection();
}

void WinVideoDestroy()
{

	// DESTROY BUFFERS
	delete [] g_pFramebufferinfo;
	g_pFramebufferinfo = NULL;

	// DESTROY FRAME BUFFER
	DeleteDC(g_hDeviceDC);
	DeleteObject(g_hDeviceBitmap);
	g_hDeviceDC = (HDC)0;
	g_hDeviceBitmap = (HBITMAP)0;
	g_pFramebufferbits = NULL;

	// DESTROY LOGO
	if (g_hLogoBitmap) {
		DeleteObject(g_hLogoBitmap);
		g_hLogoBitmap = (HBITMAP)0;
	}

	NTSC_Destroy();
}

//===========================================================================
void VideoBenchmark () {
  _ASSERT(g_nAppMode == MODE_BENCHMARK);
  Sleep(500);

  // PREPARE TWO DIFFERENT FRAME BUFFERS, EACH OF WHICH HAVE HALF OF THE
  // BYTES SET TO 0x14 AND THE OTHER HALF SET TO 0xAA
  int     loop;
  LPDWORD mem32 = (LPDWORD)mem;
  for (loop = 4096; loop < 6144; loop++)
    *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0x14141414
                                                        : 0xAAAAAAAA;
  for (loop = 6144; loop < 8192; loop++)
    *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0xAAAAAAAA
                                                        : 0x14141414;

  // SEE HOW MANY TEXT FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totaltextfps = 0;

  g_uVideoMode            = VF_TEXT;
  FillMemory(mem+0x400,0x400,0x14);
  VideoRedrawScreen();
  DWORD milliseconds = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  DWORD cycle = 0;
  do {
    if (cycle & 1)
      FillMemory(mem+0x400,0x400,0x14);
    else
      CopyMemory(mem+0x400,mem+((cycle & 2) ? 0x4000 : 0x6000),0x400);
    VideoRefreshScreen();
    if (cycle++ >= 3)
      cycle = 0;
    totaltextfps++;
  } while (GetTickCount() - milliseconds < 1000);

  // SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totalhiresfps = 0;
  g_uVideoMode             = VF_HIRES;
  FillMemory(mem+0x2000,0x2000,0x14);
  VideoRedrawScreen();
  milliseconds = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  cycle = 0;
  do {
    if (cycle & 1)
      FillMemory(mem+0x2000,0x2000,0x14);
    else
      CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoRefreshScreen();
    if (cycle++ >= 3)
      cycle = 0;
    totalhiresfps++;
  } while (GetTickCount() - milliseconds < 1000);

  // DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
  // NOTHING ELSE GOING ON
  DWORD totalmhz10[2] = {0,0};	// bVideoUpdate & !bVideoUpdate
  for (UINT i=0; i<2; i++)
  {
	  CpuSetupBenchmark();
	  milliseconds = GetTickCount();
	  while (GetTickCount() == milliseconds) ;
	  milliseconds = GetTickCount();
	  do {
		  CpuExecute(100000, i==0 ? true : false);
		totalmhz10[i]++;
	  } while (GetTickCount() - milliseconds < 1000);
  }

  // IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
  // CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
  if ((regs.pc < 0x300) || (regs.pc > 0x400))
    if (MessageBox(g_hFrameWindow,
                   TEXT("The emulator has detected a problem while running ")
                   TEXT("the CPU benchmark.  Would you like to gather more ")
                   TEXT("information?"),
                   TEXT("Benchmarks"),
                   MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES) {
      BOOL error  = 0;
      WORD lastpc = 0x300;
      int  loop   = 0;
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
        TCHAR outstr[256];
        wsprintf(outstr,
                 TEXT("The emulator experienced an error %u clock cycles ")
                 TEXT("into the CPU benchmark.  Prior to the error, the ")
                 TEXT("program counter was at $%04X.  After the error, it ")
                 TEXT("had jumped to $%04X."),
                 (unsigned)loop,
                 (unsigned)lastpc,
                 (unsigned)regs.pc);
        MessageBox(g_hFrameWindow,
                   outstr,
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
      }
      else
        MessageBox(g_hFrameWindow,
                   TEXT("The emulator was unable to locate the exact ")
                   TEXT("point of the error.  This probably means that ")
                   TEXT("the problem is external to the emulator, ")
                   TEXT("happening asynchronously, such as a problem in ")
                   TEXT("a timer interrupt handler."),
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
    }

  // DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
  // WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
  // THE SAME TIME
  DWORD realisticfps = 0;
  FillMemory(mem+0x2000,0x2000,0xAA);
  VideoRedrawScreen();
  milliseconds = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  cycle = 0;
  do {
    if (realisticfps < 10) {
      int cycles = 100000;
      while (cycles > 0) {
        DWORD executedcycles = CpuExecute(103, true);
        cycles -= executedcycles;
		GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedcycles);
        JoyUpdateButtonLatch(executedcycles);
	  }
    }
    if (cycle & 1)
      FillMemory(mem+0x2000,0x2000,0xAA);
    else
      CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoRedrawScreen();
    if (cycle++ >= 3)
      cycle = 0;
    realisticfps++;
  } while (GetTickCount() - milliseconds < 1000);

  // DISPLAY THE RESULTS
  VideoDisplayLogo();
  TCHAR outstr[256];
  wsprintf(outstr,
           TEXT("Pure Video FPS:\t%u hires, %u text\n")
           TEXT("Pure CPU MHz:\t%u.%u%s (video update)\n")
           TEXT("Pure CPU MHz:\t%u.%u%s (full-speed)\n\n")
           TEXT("EXPECTED AVERAGE VIDEO GAME\n")
           TEXT("PERFORMANCE: %u FPS"),
           (unsigned)totalhiresfps,
           (unsigned)totaltextfps,
           (unsigned)(totalmhz10[0] / 10), (unsigned)(totalmhz10[0] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)(totalmhz10[1] / 10), (unsigned)(totalmhz10[1] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)realisticfps);
  MessageBox(g_hFrameWindow,
             outstr,
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
            
// This is called from PageConfig
//===========================================================================
void VideoChooseMonochromeColor ()
{
	CHOOSECOLOR cc;
	memset(&cc, 0, sizeof(CHOOSECOLOR));
	cc.lStructSize     = sizeof(CHOOSECOLOR);
	cc.hwndOwner       = g_hFrameWindow;
	cc.rgbResult       = g_nMonochromeRGB;
	cc.lpCustColors    = customcolors + 1;
	cc.Flags           = CC_RGBINIT | CC_SOLIDCOLOR;
	if (ChooseColor(&cc))
	{
		g_nMonochromeRGB = cc.rgbResult;
		VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
		{
			VideoRedrawScreen();
		}
		Config_Save_Video();
	}
}

//===========================================================================

static void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale)
{
	HDC hSrcDC = CreateCompatibleDC( hDstDC );
	SelectObject( hSrcDC, g_hLogoBitmap );
	StretchBlt(
		hDstDC,   // hdcDest
		xoff, yoff,  // nXDest, nYDest
		scale * srcw, scale * srch, // nWidth, nHeight
		hSrcDC,   // hdcSrc
		0, 0,     // nXSrc, nYSrc
		srcw, srch,
		SRCCOPY   // dwRop
	);

	DeleteObject( hSrcDC );
}

//===========================================================================
void VideoDisplayLogo () 
{
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
			nLogoX = (g_nViewportCX - scale*bm.bmWidth )/2;
			nLogoY = (g_nViewportCY - scale*bm.bmHeight)/2;

			if( IsFullScreen() )
			{
				nLogoX += GetFullScreenOffsetX();
				nLogoY += GetFullScreenOffsetY();
			}

			VideoDrawLogoBitmap( hFrameDC, nLogoX, nLogoY, bm.bmWidth, bm.bmHeight, scale );
		}
	}

	// DRAW THE VERSION NUMBER
	TCHAR sFontName[] = TEXT("Arial");
	HFONT font = CreateFont(-20,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
							OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
							VARIABLE_PITCH | 4 | FF_SWISS,
							sFontName );
	SelectObject(hFrameDC,font);
	SetTextAlign(hFrameDC,TA_RIGHT | TA_TOP);
	SetBkMode(hFrameDC,TRANSPARENT);

	TCHAR szVersion[ 64 ];
	StringCbPrintf(szVersion, 64, "Version %s", VERSIONSTRING);
	int xoff = GetFullScreenOffsetX(), yoff = GetFullScreenOffsetY();

#define  DRAWVERSION(x,y,c)                 \
	SetTextColor(hFrameDC,c);               \
	TextOut(hFrameDC,                       \
		scale*540+x+xoff,scale*358+y+yoff,  \
		szVersion,                          \
		strlen(szVersion));

	if (GetDeviceCaps(hFrameDC,PLANES) * GetDeviceCaps(hFrameDC,BITSPIXEL) <= 4) {
		DRAWVERSION( 2, 2,RGB(0x00,0x00,0x00));
		DRAWVERSION( 1, 1,RGB(0x00,0x00,0x00));
		DRAWVERSION( 0, 0,RGB(0xFF,0x00,0xFF));
	} else {
		DRAWVERSION( 1, 1,PALETTERGB(0x30,0x30,0x70));
		DRAWVERSION(-1,-1,PALETTERGB(0xC0,0x70,0xE0));
		DRAWVERSION( 0, 0,PALETTERGB(0x70,0x30,0xE0));
	}

#if _DEBUG
	StringCbPrintf(szVersion, 64, "DEBUG");
	DRAWVERSION( 2, -358*scale,RGB(0x00,0x00,0x00));
	DRAWVERSION( 1, -357*scale,RGB(0x00,0x00,0x00));
	DRAWVERSION( 0, -356*scale,RGB(0xFF,0x00,0xFF));
#endif

#undef  DRAWVERSION

	DeleteObject(font);
}

//===========================================================================

void VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit /*=false*/)
{
	static DWORD dwFullSpeedStartTime = 0;
//	static bool bValid = false;

	if (bInit)
	{
		// Just entered full-speed mode
//		bValid = false;
		dwFullSpeedStartTime = GetTickCount();
		return;
	}

	DWORD dwFullSpeedDuration = GetTickCount() - dwFullSpeedStartTime;
	if (dwFullSpeedDuration <= 16)	// Only update after every realtime ~17ms of *continuous* full-speed
		return;

	dwFullSpeedStartTime += dwFullSpeedDuration;

	//

#if 0
	static BYTE text_main[1024*2] = {0};	// page1 & 2
	static BYTE text_aux[1024*2] = {0};		// page1 & 2
	static BYTE hgr_main[8192*2] = {0};		// page1 & 2
	static BYTE hgr_aux[8192*2] = {0};		// page1 & 2

	bool bRedraw = true;	// Always redraw for bValid==false (ie. just entered full-speed mode)

	if (bValid)
	{
		if ((g_uVideoMode&(VF_DHIRES|VF_HIRES|VF_TEXT|VF_MIXED)) == VF_HIRES)
		{
			// HIRES (not MIXED) - eg. AZTEC.DSK
			if ((g_uVideoMode&VF_PAGE2) == 0)
				bRedraw = memcmp(&hgr_main[0x0000],  MemGetMainPtr(0x2000), 8192) != 0;
			else
				bRedraw = memcmp(&hgr_main[0x2000],  MemGetMainPtr(0x4000), 8192) != 0;
		}
		else
		{
			bRedraw =
				(memcmp(text_main, MemGetMainPtr(0x400),  sizeof(text_main)) != 0) ||
				(memcmp(text_aux,  MemGetAuxPtr(0x400),   sizeof(text_aux))  != 0) ||
				(memcmp(hgr_main,  MemGetMainPtr(0x2000), sizeof(hgr_main))  != 0) ||
				(memcmp(hgr_aux,   MemGetAuxPtr(0x2000),  sizeof(hgr_aux))   != 0);
		}
	}

	if (bRedraw)
		VideoRedrawScreenAfterFullSpeed(dwCyclesThisFrame);

	// Copy all video memory (+ screen holes)
	memcpy(text_main, MemGetMainPtr(0x400),  sizeof(text_main));
	memcpy(text_aux,  MemGetAuxPtr(0x400),   sizeof(text_aux));
	memcpy(hgr_main,  MemGetMainPtr(0x2000), sizeof(hgr_main));
	memcpy(hgr_aux,   MemGetAuxPtr(0x2000),  sizeof(hgr_aux));

	bValid = true;
#else
	VideoRedrawScreenAfterFullSpeed(dwCyclesThisFrame);
#endif
}

//===========================================================================

void VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame)
{
	NTSC_VideoClockResync(dwCyclesThisFrame);
	VideoRedrawScreen();	// Better (no flicker) than using: NTSC_VideoReinitialize() or VideoReinitialize()
}

//===========================================================================

void VideoRedrawScreen (void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	VideoRefreshScreen( g_uVideoMode, true );
}

//===========================================================================

void VideoRefreshScreen ( uint32_t uRedrawWholeScreenVideoMode /* =0*/, bool bRedrawWholeScreen /* =false*/ )
{
	if (bRedrawWholeScreen || g_nAppMode == MODE_PAUSED)
	{
		// uVideoModeForWholeScreen set if:
		// . MODE_DEBUG   : always
		// . MODE_RUNNING : called from VideoRedrawScreen(), eg. during full-speed
		if (bRedrawWholeScreen)
			NTSC_SetVideoMode( uRedrawWholeScreenVideoMode );
		NTSC_VideoRedrawWholeScreen();

		// MODE_DEBUG|PAUSED: Need to refresh a 2nd time if changing video-type, otherwise could have residue from prev image!
		// . eg. Amber -> B&W TV
		if (g_nAppMode == MODE_DEBUG || g_nAppMode == MODE_PAUSED)
			NTSC_VideoRedrawWholeScreen();
	}

	HDC hFrameDC = FrameGetDC();

	if (hFrameDC)
	{
		int xSrc = GetFrameBufferBorderWidth();
		int ySrc = GetFrameBufferBorderHeight();

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
			GetFrameBufferBorderlessWidth(), GetFrameBufferBorderlessHeight(),
			SRCCOPY);
	}

#ifdef NO_DIRECT_X
#else
	//if (g_lpDD) g_lpDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
#endif // NO_DIRECT_X

	GdiFlush();
}

//===========================================================================

#define MAX_DRAW_DEVICES 10

static char *draw_devices[MAX_DRAW_DEVICES];
static GUID draw_device_guid[MAX_DRAW_DEVICES];
static int num_draw_devices = 0;
static LPDIRECTDRAW g_lpDD = NULL;

static BOOL CALLBACK DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName,  LPVOID lpContext)
{
	int i = num_draw_devices;
	if (i == MAX_DRAW_DEVICES)
		return TRUE;
	if (lpGUID != NULL)
		memcpy(&draw_device_guid[i], lpGUID, sizeof (GUID));
	draw_devices[i] = _strdup(lpszDesc);

	if (g_fh) fprintf(g_fh, "%d: %s - %s\n",i,lpszDesc,lpszDrvName);

	num_draw_devices++;
	return TRUE;
}

bool DDInit(void)
{
#ifdef NO_DIRECT_X

	return false;

#else
	HRESULT hr = DirectDrawEnumerate((LPDDENUMCALLBACK)DDEnumProc, NULL);
	if (FAILED(hr))
	{
		LogFileOutput("DSEnumerate failed (%08X)\n", hr);
		return false;
	}

	LogFileOutput("Number of draw devices = %d\n", num_draw_devices);

	bool bCreatedOK = false;
	for (int x=0; x<num_draw_devices; x++)
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

void DDUninit(void)
{
	SAFE_RELEASE(g_lpDD);
}

#undef SAFE_RELEASE

//===========================================================================

void Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename)
{
	_ASSERT(pScreenshotFilename);
	if (!pScreenshotFilename)
		return;

	VideoRedrawScreen();
	Video_SaveScreenShot(SCREENSHOT_560x384, pScreenshotFilename);
}
