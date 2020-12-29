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
#include "Interface.h"
#include "Core.h"
#include "CPU.h"
#include "Joystick.h"
#include "Log.h"
#include "Memory.h"
#include "CardManager.h"
#include "NTSC.h"

#include "../resource/resource.h"

void WinVideo::videoCreateDIBSection(void)
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
	memset(GetFrameBuffer(), 0, GetFrameBufferWidth() * GetFrameBufferHeight() * sizeof(bgra_t));

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	NTSC_VideoInit(GetFrameBuffer());
}

void WinVideo::Initialize(void)
{
	Video::Initialize();

	// RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
	VideoResetState();

	// LOAD THE LOGO
	g_hLogoBitmap = LoadBitmap(GetFrame().g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN));

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

#if 0
	DDInit();	// For WaitForVerticalBlank()
#endif
}

void WinVideo::Destroy(void)
{
	// DESTROY BUFFERS
	delete [] g_pFramebufferinfo;
	g_pFramebufferinfo = NULL;

	// DESTROY FRAME BUFFER
	DeleteDC(g_hDeviceDC);
	DeleteObject(g_hDeviceBitmap);
	g_hDeviceDC = (HDC)0;
	g_hDeviceBitmap = (HBITMAP)0;
	SetFrameBuffer(NULL);

	// DESTROY LOGO
	if (g_hLogoBitmap) {
		DeleteObject(g_hLogoBitmap);
		g_hLogoBitmap = (HBITMAP)0;
	}

	NTSC_Destroy();

	DDUninit();

	Video::Destroy();
}

//===========================================================================
void WinVideo::Benchmark(void)
{
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

  SetVideoMode(VF_TEXT);
  memset(mem+0x400,0x14,0x400);
  GetFrame().VideoRedrawScreen();
  DWORD milliseconds = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  DWORD cycle = 0;
  do {
    if (cycle & 1)
      memset(mem+0x400,0x14,0x400);
    else
      memcpy(mem+0x400,mem+((cycle & 2) ? 0x4000 : 0x6000),0x400);
    VideoPresentScreen();
    if (cycle++ >= 3)
      cycle = 0;
    totaltextfps++;
  } while (GetTickCount() - milliseconds < 1000);

  // SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totalhiresfps = 0;
  SetVideoMode(VF_HIRES);
  memset(mem+0x2000,0x14,0x2000);
  GetFrame().VideoRedrawScreen();
  milliseconds = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  cycle = 0;
  do {
    if (cycle & 1)
      memset(mem+0x2000,0x14,0x2000);
    else
      memcpy(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoPresentScreen();
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
    if (MessageBox(GetFrame().g_hFrameWindow,
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
        MessageBox(GetFrame().g_hFrameWindow,
                   outstr,
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
      }
      else
        MessageBox(GetFrame().g_hFrameWindow,
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
  memset(mem+0x2000,0xAA,0x2000);
  GetFrame().VideoRedrawScreen();
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
      memset(mem+0x2000,0xAA,0x2000);
    else
      memcpy(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
	GetFrame().VideoRedrawScreen();
    if (cycle++ >= 3)
      cycle = 0;
    realisticfps++;
  } while (GetTickCount() - milliseconds < 1000);

  // DISPLAY THE RESULTS
  DisplayLogo();
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
  MessageBox(GetFrame().g_hFrameWindow,
             outstr,
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
            
//===========================================================================

// This is called from PageConfig
void WinVideo::ChooseMonochromeColor(void)
{
	CHOOSECOLOR cc;
	memset(&cc, 0, sizeof(CHOOSECOLOR));
	cc.lStructSize     = sizeof(CHOOSECOLOR);
	cc.hwndOwner       = GetFrame().g_hFrameWindow;
	cc.rgbResult       = GetMonochromeRGB();
	cc.lpCustColors    = customcolors + 1;
	cc.Flags           = CC_RGBINIT | CC_SOLIDCOLOR;
	if (ChooseColor(&cc))
	{
		SetMonochromeRGB(cc.rgbResult);
		VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
		{
			GetFrame().VideoRedrawScreen();
		}
		Config_Save_Video();
	}
}

//===========================================================================

void WinVideo::VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale)
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

void WinVideo::DisplayLogo(void)
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

void WinVideo::VideoPresentScreen(void)
{
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

BOOL CALLBACK WinVideo::DDEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName,  LPVOID lpContext)
{
	WinVideo* obj = (WinVideo*)lpContext;

	int i = obj->num_draw_devices;
	if (i == MAX_DRAW_DEVICES)
		return TRUE;
	if (lpGUID != NULL)
		memcpy(&(obj->draw_device_guid[i]), lpGUID, sizeof (GUID));
	obj->draw_devices[i] = _strdup(lpszDesc);

	if (g_fh) fprintf(g_fh, "%d: %s - %s\n",i,lpszDesc,lpszDrvName);

	(obj->num_draw_devices)++;
	return TRUE;
}

bool WinVideo::DDInit(void)
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

void WinVideo::DDUninit(void)
{
	SAFE_RELEASE(g_lpDD);
}

#undef SAFE_RELEASE

//===========================================================================
