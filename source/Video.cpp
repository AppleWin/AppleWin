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

#include "AppleWin.h"
#include "CPU.h"
#include "Frame.h"
#include "Keyboard.h"
#include "Memory.h"
#include "Registry.h"
#include "Video.h"
#include "NTSC.h"

#include "..\resource\resource.h"
#include "Configuration\PropertySheet.h"
#include "YamlHelper.h"

	#define  SW_80COL         (g_uVideoMode & VF_80COL)
	#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
	#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
	#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
	#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
	#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
	#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

// Globals (Public)

    uint8_t      *g_pFramebufferbits = NULL; // last drawn frame
	int           g_nAltCharSetOffset  = 0; // alternate character set

// Globals (Private)

// video scanner constants
int const kHBurstClock      =    53; // clock when Color Burst starts
int const kHBurstClocks     =     4; // clocks per Color Burst duration
int const kHClock0State     =  0x18; // H[543210] = 011000
int const kHClocks          =    65; // clocks per horizontal scan (including HBL)
int const kHPEClock         =    40; // clock when HPE (horizontal preset enable) goes low
int const kHPresetClock     =    41; // clock when H state presets
int const kHSyncClock       =    49; // clock when HSync starts
int const kHSyncClocks      =     4; // clocks per HSync duration
int const kNTSCScanLines    =   262; // total scan lines including VBL (NTSC)
int const kNTSCVSyncLine    =   224; // line when VSync starts (NTSC)
int const kPALScanLines     =   312; // total scan lines including VBL (PAL)
int const kPALVSyncLine     =   264; // line when VSync starts (PAL)
int const kVLine0State      = 0x100; // V[543210CBA] = 100000000
int const kVPresetLine      =   256; // line when V state presets
int const kVSyncLines       =     4; // lines per VSync duration
int const kVDisplayableScanLines = 192; // max displayable scanlines

static COLORREF      customcolors[256];	// MONOCHROME is last custom color

static HBITMAP       g_hDeviceBitmap;
static HDC           g_hDeviceDC;
static LPBITMAPINFO  g_pFramebufferinfo = NULL;

       HBITMAP       g_hLogoBitmap;

const int MAX_SOURCE_Y = 512;
static LPBYTE        g_aSourceStartofLine[ MAX_SOURCE_Y ];

COLORREF         g_nMonochromeRGB    = RGB(0xC0,0xC0,0xC0);

uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

DWORD     g_eVideoType     = VT_COLOR_TV;
DWORD     g_uHalfScanLines = 1; // drop 50% scan lines for a more authentic look

static const bool bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

//-------------------------------------

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	TCHAR g_aVideoChoices[] =
		TEXT("Monochrome (Custom)\0")
		TEXT("Color Monitor\0")
		TEXT("B&W TV\0")
		TEXT("Color TV\0")
		TEXT("Monochrome (Amber)\0")
		TEXT("Monochrome (Green)\0")
		TEXT("Monochrome (White)\0")
		;

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	// The window title will be set to this.
	char *g_apVideoModeDesc[ NUM_VIDEO_MODES ] =
	{
		  "Monochrome Monitor (Custom)"
		, "Color Monitor"
		, "B&W TV"
		, "Color TV"
		, "Amber Monitor"
		, "Green Monitor"
		, "White Monitor"
	};

// Prototypes (Private) _____________________________________________

	bool g_bDisplayPrintScreenFileName = false;
	bool g_bShowPrintScreenWarningDialog = true;
	void Util_MakeScreenShotFileName( char *pFinalFileName_ );
	bool Util_TestScreenShotFileName( const char *pFileName );
	void Video_SaveScreenShot( const char *pScreenShotFileName, const VideoScreenShot_e ScreenShotType );
	void Video_MakeScreenShot( FILE *pFile, const VideoScreenShot_e ScreenShotType );
	void videoCreateDIBSection();

//===========================================================================
void VideoInitialize ()
{
	// RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
	VideoResetState();

	// LOAD THE LOGO
	g_hLogoBitmap = LoadBitmap( g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN) );

	// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
	g_pFramebufferinfo = (LPBITMAPINFO)VirtualAlloc(
		NULL,
		sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD),
		MEM_COMMIT,
		PAGE_READWRITE);

	ZeroMemory(g_pFramebufferinfo,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
	g_pFramebufferinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	g_pFramebufferinfo->bmiHeader.biWidth       = GetFrameBufferWidth();
	g_pFramebufferinfo->bmiHeader.biHeight      = GetFrameBufferHeight();
	g_pFramebufferinfo->bmiHeader.biPlanes      = 1;
	g_pFramebufferinfo->bmiHeader.biBitCount    = 32;
	g_pFramebufferinfo->bmiHeader.biCompression = BI_RGB;
	g_pFramebufferinfo->bmiHeader.biClrUsed     = 0;

	videoCreateDIBSection();
}

//===========================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void VideoBenchmark () {
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
  CpuSetupBenchmark();
  DWORD totalmhz10 = 0;
  milliseconds     = GetTickCount();
  while (GetTickCount() == milliseconds) ;
  milliseconds = GetTickCount();
  cycle = 0;
  do {
    CpuExecute(100000, true);
    totalmhz10++;
  } while (GetTickCount() - milliseconds < 1000);

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
        DiskUpdatePosition(executedcycles);
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
           TEXT("Pure CPU MHz:\t%u.%u%s\n\n")
           TEXT("EXPECTED AVERAGE VIDEO GAME\n")
           TEXT("PERFORMANCE: %u FPS"),
           (unsigned)totalhiresfps,
           (unsigned)totaltextfps,
           (unsigned)(totalmhz10/10),
           (unsigned)(totalmhz10 % 10),
           (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)realisticfps);
  MessageBox(g_hFrameWindow,
             outstr,
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
            
//===========================================================================
BYTE VideoCheckMode (WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
  address &= 0xFF;
  if (address == 0x7F)
    return MemReadFloatingBus(SW_DHIRES != 0, uExecutedCycles);
  else {
    BOOL result = 0;
    switch (address) {
      case 0x1A: result = SW_TEXT;    break;
      case 0x1B: result = SW_MIXED;   break;
      case 0x1D: result = SW_HIRES;   break;
      case 0x1E: result = g_nAltCharSetOffset;   break;
      case 0x1F: result = SW_80COL;   break;
      case 0x7F: result = SW_DHIRES;  break;
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
  }
}

//===========================================================================
BYTE VideoCheckVbl ( ULONG uExecutedCycles )
{
	bool bVblBar = VideoGetVblBar(uExecutedCycles);
	BYTE r = KeybGetKeycode();
	return (r & ~0x80) | (bVblBar ? 0x80 : 0);
 }

// This is called from PageConfig
//===========================================================================
void VideoChooseMonochromeColor ()
{
	CHOOSECOLOR cc;
	ZeroMemory(&cc,sizeof(CHOOSECOLOR));
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
void VideoDestroy () {

  // DESTROY BUFFERS
  VirtualFree(g_pFramebufferinfo,0,MEM_RELEASE);
  g_pFramebufferinfo = NULL;

  // DESTROY FRAME BUFFER
  DeleteDC(g_hDeviceDC);
  DeleteObject(g_hDeviceBitmap);
  g_hDeviceDC     = (HDC)0;
  g_hDeviceBitmap = (HBITMAP)0;

  // DESTROY LOGO
  if (g_hLogoBitmap) {
    DeleteObject(g_hLogoBitmap);
    g_hLogoBitmap = (HBITMAP)0;
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

	char szVersion[ 64 ] = "";
	sprintf( szVersion, "Version %s", VERSIONSTRING );
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
	sprintf( szVersion, "DEBUG" );
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
	const int nScanLines = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;

	g_nVideoClockVert = (uint16_t) (dwCyclesThisFrame / kHClocks) % nScanLines;
	g_nVideoClockHorz = (uint16_t) (dwCyclesThisFrame % kHClocks);

	VideoRedrawScreen();	// Better (no flicker) than using: NTSC_VideoReinitialize() or VideoReinitialize()
}

//===========================================================================

void VideoRedrawScreen (void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	VideoRefreshScreen( g_uVideoMode, true );
}

//===========================================================================

// NB. Can get "big" 1000+ms times: these occur during disk loading when the emulator is at full-speed.

//#define DEBUG_REFRESH_TIMINGS

#if defined(_DEBUG) && defined(DEBUG_REFRESH_TIMINGS)
static void DebugRefresh(char uDebugFlag)
{
	static DWORD uLastRefreshTime = 0;

	const DWORD dwEmuTime_ms = CpuGetEmulationTime_ms();
	const DWORD uTimeBetweenRefreshes = uLastRefreshTime ? dwEmuTime_ms - uLastRefreshTime : 0;
	uLastRefreshTime = dwEmuTime_ms;

	if (!uTimeBetweenRefreshes)
		return;					// 1st time in func

	char szStr[100];
	sprintf(szStr, "Time between refreshes = %d ms %c\n", uTimeBetweenRefreshes, (uDebugFlag==0)?' ':uDebugFlag);
	OutputDebugString(szStr);
}
#endif

// TC: Hacky-fix for GH#341 - better to draw to the correct position in the framebuffer to start with! (in NTSC.cpp)
static void VideoFrameBufferAdjust(int& xSrc, int& ySrc, bool bInvertY=false)
{
	int dx=0, dy=0;

	if (g_eVideoType == VT_MONO_TV || g_eVideoType == VT_COLOR_TV)
	{
		// Adjust the src locations for the NTSC video modes
		dx = 2;
		dy = -1;
	}
	else if (g_eVideoType == VT_COLOR_MONITOR)
	{
		//if ((g_uVideoMode & VF_TEXT) == 0)	// NB. Not sufficient, eg. ANSI STORY...
		if ( NTSC_GetColorBurst() == true )	// ANSI STORY (end credits): split DGR/TEXT80/DGR on scanline
			dx = 2;
	}

	if (bInvertY)
		dy =- dy;

	xSrc += dx;
	ySrc += dy;
}

void VideoRefreshScreen ( uint32_t uRedrawWholeScreenVideoMode /* =0*/, bool bRedrawWholeScreen /* =false*/ )
{
#if defined(_DEBUG) && defined(DEBUG_REFRESH_TIMINGS)
	DebugRefresh(0);
#endif

	if (bRedrawWholeScreen || g_nAppMode == MODE_PAUSED)
	{
		// uVideoModeForWholeScreen set if:
		// . MODE_DEBUG   : always
		// . MODE_RUNNING : called from VideoRedrawScreen(), eg. during full-speed
		if (bRedrawWholeScreen)
			NTSC_SetVideoMode( uRedrawWholeScreenVideoMode );
		NTSC_VideoRedrawWholeScreen();
	}

	HDC hFrameDC = FrameGetDC();

	if (hFrameDC)
	{
		int xSrc = GetFrameBufferBorderWidth();
		int ySrc = GetFrameBufferBorderHeight();
		VideoFrameBufferAdjust(xSrc, ySrc);	// TC: Hacky-fix for GH#341

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

	GdiFlush();
}

//===========================================================================
void VideoReinitialize ()
{
	NTSC_VideoReinitialize( g_dwCyclesThisFrame );
	NTSC_VideoInitAppleType();
	NTSC_SetVideoStyle();
	NTSC_SetVideoTextMode( g_uVideoMode &  VF_80COL ? 80 : 40 );
	NTSC_SetVideoMode( g_uVideoMode );	// Pre-condition: g_nVideoClockHorz (derived from g_dwCyclesThisFrame)
}

//===========================================================================
void VideoResetState ()
{
	g_nAltCharSetOffset    = 0;
	g_uVideoMode           = VF_TEXT;

	NTSC_SetVideoTextMode( 40 );
	NTSC_SetVideoMode( g_uVideoMode );
}


//===========================================================================

BYTE VideoSetMode (WORD, WORD address, BYTE write, BYTE, ULONG uExecutedCycles)
{
	address &= 0xFF;

	switch (address)
	{
		case 0x00:                 g_uVideoMode &= ~VF_80STORE;                            break;
		case 0x01:                 g_uVideoMode |=  VF_80STORE;                            break;
		case 0x0C: if (!IS_APPLE2){g_uVideoMode &= ~VF_80COL; NTSC_SetVideoTextMode(40);}; break;
		case 0x0D: if (!IS_APPLE2){g_uVideoMode |=  VF_80COL; NTSC_SetVideoTextMode(80);}; break;
		case 0x0E: if (!IS_APPLE2) g_nAltCharSetOffset = 0;           break;	// Alternate char set off
		case 0x0F: if (!IS_APPLE2) g_nAltCharSetOffset = 256;         break;	// Alternate char set on
		case 0x50: g_uVideoMode &= ~VF_TEXT;    break;
		case 0x51: g_uVideoMode |=  VF_TEXT;    break;
		case 0x52: g_uVideoMode &= ~VF_MIXED;   break;
		case 0x53: g_uVideoMode |=  VF_MIXED;   break;
		case 0x54: g_uVideoMode &= ~VF_PAGE2;   break;
		case 0x55: g_uVideoMode |=  VF_PAGE2;   break;
		case 0x56: g_uVideoMode &= ~VF_HIRES;   break;
		case 0x57: g_uVideoMode |=  VF_HIRES;   break;
		case 0x5E: if (!IS_APPLE2) g_uVideoMode |=  VF_DHIRES;  break;
		case 0x5F: if (!IS_APPLE2) g_uVideoMode &= ~VF_DHIRES;  break;
	}

	NTSC_SetVideoMode( g_uVideoMode );

	return MemReadFloatingBus(uExecutedCycles);
}

//===========================================================================

bool VideoGetSW80COL(void)
{
	return SW_80COL ? true : false;
}

bool VideoGetSWDHIRES(void)
{
	return SW_DHIRES ? true : false;
}

bool VideoGetSWHIRES(void)
{
	return SW_HIRES ? true : false;
}

bool VideoGetSW80STORE(void)
{
	return SW_80STORE ? true : false;
}

bool VideoGetSWMIXED(void)
{
	return SW_MIXED ? true : false;
}

bool VideoGetSWPAGE2(void)
{
	return SW_PAGE2 ? true : false;
}

bool VideoGetSWTEXT(void)
{
	return SW_TEXT ? true : false;
}

bool VideoGetSWAltCharSet(void)
{
	return g_nAltCharSetOffset != 0;
}

//===========================================================================

void VideoSetSnapshot_v1(const UINT AltCharSet, const UINT VideoMode)
{
	g_nAltCharSetOffset = !AltCharSet ? 0 : 256;
	g_uVideoMode = VideoMode;
	g_dwCyclesThisFrame = 0;
}

//

#define SS_YAML_KEY_ALTCHARSET "Alt Char Set"
#define SS_YAML_KEY_VIDEOMODE "Video Mode"
#define SS_YAML_KEY_CYCLESTHISFRAME "Cycles This Frame"

static std::string VideoGetSnapshotStructName(void)
{
	static const std::string name("Video");
	return name;
}

void VideoSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", VideoGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveBool(SS_YAML_KEY_ALTCHARSET, g_nAltCharSetOffset ? true : false);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_VIDEOMODE, g_uVideoMode);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_CYCLESTHISFRAME, g_dwCyclesThisFrame);
}

void VideoLoadSnapshot(YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap(VideoGetSnapshotStructName()))
		return;

	g_nAltCharSetOffset = yamlLoadHelper.LoadBool(SS_YAML_KEY_ALTCHARSET) ? 256 : 0;
	g_uVideoMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_VIDEOMODE);
	g_dwCyclesThisFrame = yamlLoadHelper.LoadUint(SS_YAML_KEY_CYCLESTHISFRAME);

	yamlLoadHelper.PopMap();
}

//===========================================================================
//
// References to Jim Sather's books are given as eg:
// UTAIIe:5-7,P3 (Understanding the Apple IIe, chapter 5, page 7, Paragraph 3)
//
WORD VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles)
{
    // get video scanner position
    //
    int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

    // machine state switches
    //
    int nHires   = (SW_HIRES && !SW_TEXT) ? 1 : 0;
    int nPage2   = SW_PAGE2 ? 1 : 0;
    int n80Store = SW_80STORE ? 1 : 0;

    // calculate video parameters according to display standard
    //
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
    int nVSyncLine  = bVideoScannerNTSC ? kNTSCVSyncLine : kPALVSyncLine;
    int nScanCycles = nScanLines * kHClocks;
    nCycles %= nScanCycles;

    // calculate horizontal scanning state
    //
    int nHClock = (nCycles + kHPEClock) % kHClocks; // which horizontal scanning clock
    int nHState = kHClock0State + nHClock; // H state bits
    if (nHClock >= kHPresetClock) // check for horizontal preset
    {
        nHState -= 1; // correct for state preset (two 0 states)
    }
    int h_0 = (nHState >> 0) & 1; // get horizontal state bits
    int h_1 = (nHState >> 1) & 1;
    int h_2 = (nHState >> 2) & 1;
    int h_3 = (nHState >> 3) & 1;
    int h_4 = (nHState >> 4) & 1;
    int h_5 = (nHState >> 5) & 1;

    // calculate vertical scanning state
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_A = (nVState >> 0) & 1; // get vertical state bits
    int v_B = (nVState >> 1) & 1;
    int v_C = (nVState >> 2) & 1;
    int v_0 = (nVState >> 3) & 1;
    int v_1 = (nVState >> 4) & 1;
    int v_2 = (nVState >> 5) & 1;
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;
    int v_5 = (nVState >> 8) & 1;

    // calculate scanning memory address
    //
    if (nHires && SW_MIXED && v_4 && v_2) // HIRES TIME signal (UTAIIe:5-7,P3)
    {
        nHires = 0; // address is in text memory for mixed hires
    }

    int nAddend0 = 0x0D; // 1            1            0            1
    int nAddend1 =              (h_5 << 2) | (h_4 << 1) | (h_3 << 0);
    int nAddend2 = (v_4 << 3) | (v_3 << 2) | (v_4 << 1) | (v_3 << 0);
    int nSum     = (nAddend0 + nAddend1 + nAddend2) & 0x0F; // SUM (UTAIIe:5-9)

    int nAddress = 0; // build address from video scanner equations (UTAIIe:5-8,T5.1)
    nAddress |= h_0  << 0; // a0
    nAddress |= h_1  << 1; // a1
    nAddress |= h_2  << 2; // a2
    nAddress |= nSum << 3; // a3 - a6
    nAddress |= v_0  << 7; // a7
    nAddress |= v_1  << 8; // a8
    nAddress |= v_2  << 9; // a9

    int p2a = !(nPage2 && !n80Store);
    int p2b = nPage2 && !n80Store;

    if (nHires) // hires?
    {
        // Y: insert hires-only address bits
        //
        nAddress |= v_A << 10; // a10
        nAddress |= v_B << 11; // a11
        nAddress |= v_C << 12; // a12
        nAddress |= p2a << 13; // a13
        nAddress |= p2b << 14; // a14
    }
    else
    {
        // N: insert text-only address bits
        //
        nAddress |= p2a << 10; // a10
        nAddress |= p2b << 11; // a11

        // Apple ][ (not //e) and HBL?
		//
		if (IS_APPLE2 && // Apple II only (UTAIIe:I-4,#5)
			!h_5 && (!h_4 || !h_3)) // HBL (UTAIIe:8-10,F8.5)
        {
            nAddress |= 1 << 12; // Y: a12 (add $1000 to address!)
        }
    }

    // update VBL' state
    //
	if (pbVblBar_OUT != NULL)
	{
		*pbVblBar_OUT = !v_4 || !v_3; // VBL' = (v_4 & v_3)' (UTAIIe:5-10,#3)
	}
    return static_cast<WORD>(nAddress);
}

//===========================================================================

bool VideoGetVblBar(const DWORD uExecutedCycles)
{
	// get video scanner position
	int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

	// calculate video parameters according to display standard
	const int kScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
	const int kScanCycles = kScanLines * kHClocks;
	nCycles %= kScanCycles;

	// VBL'
	return nCycles < kVDisplayableScanLines * kHClocks;
}

//===========================================================================

#define SCREENSHOT_BMP 1
#define SCREENSHOT_TGA 0
	
static int  g_nLastScreenShot = 0;
const  int nMaxScreenShot = 999999999;
static char *g_pLastDiskImageName = NULL;

//===========================================================================
void Video_ResetScreenshotCounter( char *pImageName )
{
	g_nLastScreenShot = 0;
	g_pLastDiskImageName = pImageName;
}

//===========================================================================
void Util_MakeScreenShotFileName( char *pFinalFileName_ )
{
	char sPrefixScreenShotFileName[ 256 ] = "AppleWin_ScreenShot";
	// TODO: g_sScreenshotDir
	char *pPrefixFileName = g_pLastDiskImageName ? g_pLastDiskImageName : sPrefixScreenShotFileName;
#if SCREENSHOT_BMP
	sprintf( pFinalFileName_, "%s_%09d.bmp", pPrefixFileName, g_nLastScreenShot );
#endif
#if SCREENSHOT_TGA
	sprintf( pFinalFileName_, "%s%09d.tga", pPrefixFileName, g_nLastScreenShot );
#endif
}

// Returns TRUE if file exists, else FALSE
//===========================================================================
bool Util_TestScreenShotFileName( const char *pFileName )
{
	bool bFileExists = false;
	FILE *pFile = fopen( pFileName, "rt" );
	if (pFile)
	{
		fclose( pFile );
		bFileExists = true;
	}
	return bFileExists;
}

//===========================================================================
void Video_TakeScreenShot( const VideoScreenShot_e ScreenShotType )
{
	char sScreenShotFileName[ MAX_PATH ];

	// find last screenshot filename so we don't overwrite the existing user ones
	bool bExists = true;
	while( bExists )
	{
		if (g_nLastScreenShot > nMaxScreenShot) // Holy Crap! User has maxed the number of screenshots!?
		{
			sprintf( sScreenShotFileName, "You have more then %d screenshot filenames!  They will no longer be saved.\n\nEither move some of your screenshots or increase the maximum in video.cpp\n", nMaxScreenShot );
			MessageBox( g_hFrameWindow, sScreenShotFileName, "Warning", MB_OK );
			g_nLastScreenShot = 0;
			return;
		}

		Util_MakeScreenShotFileName( sScreenShotFileName );
		bExists = Util_TestScreenShotFileName( sScreenShotFileName );
		if( !bExists )
		{
			break;
		}
		g_nLastScreenShot++;
	}

	Video_SaveScreenShot( sScreenShotFileName, ScreenShotType );
	g_nLastScreenShot++;
}

WinBmpHeader_t g_tBmpHeader;

#if SCREENSHOT_TGA
	enum TargaImageType_e
	{
		TARGA_RGB	= 2
	};

	struct TargaHeader_t
	{										// Addr Bytes
		u8		nIdBytes					; // 00 01 size of ID field that follows 18 byte header (0 usually)
		u8		bHasPalette				; // 01 01
		u8		iImageType				; // 02 01 type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

		s16	iPaletteFirstColor	; // 03 02
		s16	nPaletteColors			; // 05 02
		u8		nPaletteBitsPerEntry	; // 07 01 number of bits per palette entry 15,16,24,32

		s16	nOriginX					; // 08 02 image x origin
		s16	nOriginY					; // 0A 02 image y origin
		s16	nWidthPixels			; // 0C 02
		s16	nHeightPixels			; // 0E 02
		u8		nBitsPerPixel			; // 10 01 image bits per pixel 8,16,24,32
		u8		iDescriptor				; // 11 01 image descriptor bits (vh flip bits)
	    
		// pixel data...
		u8		aPixelData[1]		; // rgb
	};

	TargaHeader_t g_tTargaHeader;
#endif // SCREENSHOT_TGA

void Video_SetBitmapHeader( WinBmpHeader_t *pBmp, int nWidth, int nHeight, int nBitsPerPixel )
{
#if SCREENSHOT_BMP
	pBmp->nCookie[ 0 ]     = 'B'; // 0x42
	pBmp->nCookie[ 1 ]     = 'M'; // 0x4d
	pBmp->nSizeFile        = 0;
	pBmp->nReserved1       = 0;
	pBmp->nReserved2       = 0;
#if VIDEO_SCREENSHOT_PALETTE
	pBmp->nOffsetData      = sizeof(WinBmpHeader_t) + (256 * sizeof(bgra_t));
#else
	pBmp->nOffsetData      = sizeof(WinBmpHeader_t);
#endif
	pBmp->nStructSize      = 0x28; // sizeof( WinBmpHeader_t );
	pBmp->nWidthPixels     = nWidth;
	pBmp->nHeightPixels    = nHeight;
	pBmp->nPlanes          = 1;
#if VIDEO_SCREENSHOT_PALETTE
	pBmp->nBitsPerPixel    = 8;
#else
	pBmp->nBitsPerPixel    = nBitsPerPixel;
#endif
	pBmp->nCompression     = BI_RGB; // none
	pBmp->nSizeImage       = 0;
	pBmp->nXPelsPerMeter   = 0;
	pBmp->nYPelsPerMeter   = 0;
#if VIDEO_SCREENSHOT_PALETTE
	pBmp->nPaletteColors   = 256;
#else
	pBmp->nPaletteColors   = 0;
#endif
	pBmp->nImportantColors = 0;
}

//===========================================================================
static void Video_MakeScreenShot(FILE *pFile, const VideoScreenShot_e ScreenShotType)
{
	WinBmpHeader_t *pBmp = &g_tBmpHeader;

	Video_SetBitmapHeader(
		pBmp,
		ScreenShotType == SCREENSHOT_280x192 ? GetFrameBufferBorderlessWidth()/2 : GetFrameBufferBorderlessWidth(),
		ScreenShotType == SCREENSHOT_280x192 ? GetFrameBufferBorderlessHeight()/2 : GetFrameBufferBorderlessHeight(),
		32
	);

//	char sText[256];
//	sprintf( sText, "sizeof: BITMAPFILEHEADER = %d\n", sizeof(BITMAPFILEHEADER) ); // = 14
//	MessageBox( g_hFrameWindow, sText, "Info 1", MB_OK );
//	sprintf( sText, "sizeof: BITMAPINFOHEADER = %d\n", sizeof(BITMAPINFOHEADER) ); // = 40
//	MessageBox( g_hFrameWindow, sText, "Info 2", MB_OK );

	char sIfSizeZeroOrUnknown_BadWinBmpHeaderPackingSize54[ sizeof( WinBmpHeader_t ) == (14 + 40) ];
	/**/ sIfSizeZeroOrUnknown_BadWinBmpHeaderPackingSize54[0]=0;

	// Write Header
	fwrite( pBmp, sizeof( WinBmpHeader_t ), 1, pFile );

	uint32_t *pSrc;
#if VIDEO_SCREENSHOT_PALETTE
	// Write Palette Data
	pSrc = ((uint8_t*)g_pFramebufferinfo) + sizeof(BITMAPINFOHEADER);
	int nLen = g_tBmpHeader.nPaletteColors * sizeof(bgra_t); // RGBQUAD
	fwrite( pSrc, nLen, 1, pFile );
	pSrc += nLen;
#endif

	// Write Pixel Data
	// No need to use GetDibBits() since we already have http://msdn.microsoft.com/en-us/library/ms532334.aspx
	// @reference: "Storing an Image" http://msdn.microsoft.com/en-us/library/ms532340(VS.85).aspx
	pSrc = (uint32_t*) g_pFramebufferbits;

	int xSrc = GetFrameBufferBorderWidth();
	int ySrc = GetFrameBufferBorderHeight();
	VideoFrameBufferAdjust(xSrc, ySrc, true);	// TC: Hacky-fix for GH#341 & GH#356
												// Lines stored in reverse, so invert the y-adjust value

	pSrc += xSrc;								// Skip left border
	pSrc += ySrc * GetFrameBufferWidth();		// Skip top border

	if( ScreenShotType == SCREENSHOT_280x192 )
	{
		pSrc += GetFrameBufferWidth();	// Start on odd scanline (otherwise for 50% scanline mode get an all black image!)

		uint32_t  aScanLine[ 280 ];
		uint32_t *pDst;

		// 50% Half Scan Line clears every odd scanline.
		// SHIFT+PrintScreen saves only the even rows.
		// NOTE: Keep in sync with _Video_RedrawScreen() & Video_MakeScreenShot()
		for( UINT y = 0; y < GetFrameBufferBorderlessHeight()/2; y++ )
		{
			pDst = aScanLine;
			for( UINT x = 0; x < GetFrameBufferBorderlessWidth()/2; x++ )
			{
				*pDst++ = pSrc[1]; // correction for left edge loss of scaled scanline [Bill Buckel, B#18928]
				pSrc += 2; // skip odd pixels
			}
			fwrite( aScanLine, sizeof(uint32_t), GetFrameBufferBorderlessWidth()/2, pFile );
			pSrc += GetFrameBufferWidth();			// scan lines doubled - skip odd ones
			pSrc += GetFrameBufferBorderWidth()*2;	// Skip right border & next line's left border
		}
	}
	else
	{
		for( UINT y = 0; y < GetFrameBufferBorderlessHeight(); y++ )
		{
			fwrite( pSrc, sizeof(uint32_t), GetFrameBufferBorderlessWidth(), pFile );
			pSrc += GetFrameBufferWidth();
		}
	}
#endif // SCREENSHOT_BMP

#if SCREENSHOT_TGA
	TargaHeader_t *pHeader = &g_tTargaHeader;
	memset( (void*)pHeader, 0, sizeof( TargaHeader_t ) );

	pHeader->iImageType    = TARGA_RGB;
	pHeader->nWidthPixels  = FRAMEBUFFER_W;
	pHeader->nHeightPixels = FRAMEBUFFER_H;
	pHeader->nBitsPerPixel = 24;
#endif // SCREENSHOT_TGA

}

//===========================================================================
static void Video_SaveScreenShot( const char *pScreenShotFileName, const VideoScreenShot_e ScreenShotType )
{
	FILE *pFile = fopen( pScreenShotFileName, "wb" );
	if( pFile )
	{
		Video_MakeScreenShot( pFile, ScreenShotType );
		fclose( pFile );
	}

	if( g_bDisplayPrintScreenFileName )
	{
		MessageBox( g_hFrameWindow, pScreenShotFileName, "Screen Captured", MB_OK );
	}
}

//===========================================================================

void Config_Load_Video()
{
	REGLOAD(TEXT(REGVALUE_VIDEO_MODE           ),&g_eVideoType);
	REGLOAD(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES),&g_uHalfScanLines);
	REGLOAD(TEXT(REGVALUE_VIDEO_MONO_COLOR     ),&g_nMonochromeRGB);

	if (g_eVideoType >= NUM_VIDEO_MODES)
		g_eVideoType = VT_COLOR_MONITOR;
}

void Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE           ),g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES),g_uHalfScanLines);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR     ),g_nMonochromeRGB);
}

// ____________________________________________________________________

//===========================================================================
static void videoCreateDIBSection()
{
	// CREATE THE DEVICE CONTEXT
	HWND window  = GetDesktopWindow();
	HDC dc       = GetDC(window);
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
			(LPVOID *)&g_pFramebufferbits,0,0
		);
	SelectObject(g_hDeviceDC,g_hDeviceBitmap);

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	ZeroMemory( g_pFramebufferbits, GetFrameBufferWidth()*GetFrameBufferHeight()*sizeof(bgra_t) );

	NTSC_VideoInit( g_pFramebufferbits );
}
