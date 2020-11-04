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

#include "Applewin.h"
#include "CPU.h"
#include "Disk.h"		// DiskUpdateDriveState()
#include "Frame.h"
#include "gamelink/gamelink.h"
#include "Keyboard.h"
#include "Log.h"
#include "Memory.h"
#include "Registry.h"
#include "Video.h"
#include "NTSC.h"
#include "RGBMonitor.h"

#include "../resource/resource.h"
#include "Configuration/PropertySheet.h"
#include "YamlHelper.h"

#include <Windows.h>		// RIK -- to inject the incoming Gamelink inputs into Applewin
#include "MouseInterface.h"	// RIK -- for Gamelink in and out
#include "Speaker.h"		// RIK -- for Gamelink::In()
#include "Mockingboard.h"	// RIK -- TODO should probably be moved somewhere other than Video.cpp !!!
#include <map>
// RIK BEGIN
// This map is to find the virtual keyboard codes given the scancodes
// because keybd_event needs both the virtual and scancodes. Totally useless.
struct VirtualKeys {
	static std::map<int, int> create_map()
	{
		std::map<int, int> m;
		m[1] = VK_BACK;
		m[3] = VK_TAB;
		m[5] = 6;
		return m;
	}
	static const std::map<int, int> g_mScanCodeMap;

};

const std::map<int, int> VirtualKeys::g_mScanCodeMap = VirtualKeys::create_map();

// RIK END


	#define  SW_80COL         (g_uVideoMode & VF_80COL)
	#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
	#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
	#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
	#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
	#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
	#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

// Globals (Public)

    uint8_t      *g_pFramebufferbits = NULL; // last drawn frame
	uint8_t		 *g_pReorderedFramebufferbits = new uint8_t[GetFrameBufferWidth() * GetFrameBufferHeight() * sizeof(bgra_t)]; // the frame realigned properly	// RIK
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

COLORREF         g_nMonochromeRGB    = RGB(0xC0,0xC0,0xC0);

uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

DWORD     g_eVideoType     = VT_DEFAULT;
static VideoStyle_e g_eVideoStyle = VS_HALF_SCANLINES;

static bool g_bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

static LPDIRECTDRAW g_lpDD = NULL;

// RIK BEGIN
// The GameLink I/O structure
struct Gamelink_Block {
	// UINT pitch;	// TODO: not needed?
	GameLink::sSharedMMapInput_R2 input_prev;
	GameLink::sSharedMMapInput_R2 input;
	GameLink::sSharedMMapAudio_R1 audio;
	bool want_mouse;
};

Gamelink_Block g_gamelink;
// RIK END




//-------------------------------------

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	TCHAR g_aVideoChoices[] =
		TEXT("Monochrome (Custom)\0")
		TEXT("Color (Composite Idealized)\0")		// newly added
		TEXT("Color (RGB Card/Monitor)\0")	// was "Color (RGB Monitor)"
		TEXT("Color (Composite Monitor)\0")	// was "Color (NTSC Monitor)"
		TEXT("Color TV\0")
		TEXT("B&W TV\0")
		TEXT("Monochrome (Amber)\0")
		TEXT("Monochrome (Green)\0")
		TEXT("Monochrome (White)\0")
		;

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	// The window title will be set to this.
	const char *g_apVideoModeDesc[ NUM_VIDEO_MODES ] =
	{
		  "Monochrome (Custom)"
		, "Color (Composite Idealized)"
		, "Color (RGB Card/Monitor)"
		, "Color (Composite Monitor)"
		, "Color TV"
		, "B&W TV"
		, "Monochrome (Amber)"
		, "Monochrome (Green)"
		, "Monochrome (White)"
	};

// Prototypes (Private) _____________________________________________

	bool g_bDisplayPrintScreenFileName = false;
	bool g_bShowPrintScreenWarningDialog = true;
	void Util_MakeScreenShotFileName( TCHAR *pFinalFileName_, DWORD chars );
	bool Util_TestScreenShotFileName( const TCHAR *pFileName );
	void Video_SaveScreenShot( const VideoScreenShot_e ScreenShotType, const TCHAR *pScreenShotFileName );
	void Video_MakeScreenShot( FILE *pFile, const VideoScreenShot_e ScreenShotType );
	void videoCreateDIBSection();
	void reverseScanlines(uint8_t* destination, uint8_t* source, uint32_t width, uint32_t height, uint8_t depth);	// RIK


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

	// RIK START
	// TODO: Might want to put this somewhere else
	if (GameLink::GetGameLinkEnabled())
	{
		// initialize the gamelink previous input to 0
		memset(&g_gamelink.input_prev, 0, sizeof(GameLink::sSharedMMapInput_R2));
	}
	// RIK END

}

//===========================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

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

  NTSC_Destroy();
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


// RIK BEGIN
// The framebuffer has its scanlines inverted, from bottom to top
// To send a correct bitmap out to a 3rd party program we need to reverse the scanlines
static void reverseScanlines (uint8_t *destination, uint8_t *source, uint32_t width, uint32_t height, uint8_t depth)
{
	uint32_t linesize = width * depth;
	uint8_t* loln = source;
	uint8_t* hiln = destination + (height - 1) * linesize;	// first pixel of the last line
	for (size_t i = 0; i < height; i++)
	{
		memcpy(hiln, loln, linesize);
		loln = loln + linesize;
		hiln = hiln - linesize;
	}
}
// RIK END

//===========================================================================

void VideoRedrawScreen (void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	VideoRefreshScreen( g_uVideoMode, true );
}

void VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode /* =0*/, bool bRedrawWholeScreen /* =false*/)
{
	if (bRedrawWholeScreen || g_nAppMode == MODE_PAUSED)
	{
		// uVideoModeForWholeScreen set if:
		// . MODE_DEBUG   : always
		// . MODE_RUNNING : called from VideoRedrawScreen(), eg. during full-speed
		if (bRedrawWholeScreen)
			NTSC_SetVideoMode(uRedrawWholeScreenVideoMode);
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

	// RIK BEGIN

	if (GameLink::GetGameLinkEnabled()) {
		// here send the last drawn frame to GameLink
		// We could efficiently send to GameLink g_pFramebufferbits with GetFrameBufferWidth/GetFrameBufferHeight, but the scanlines are reversed
		// We instead memcpy each scanline of the bitmap of the frame in reverse into another buffer, and pass that to GameLink.
		// When GridCartographer/GameLink allows to pass in flags specifying the x/y/w/h etc...,
		// Then we can go back to using g_pFramebufferbits and let GC handle it efficiently on its end, as it uses the GPU
		//
		// TODO: See if there is an easy function for this in the Win API or if there's another faster way

		if (g_pFramebufferbits != NULL)
		{
			reverseScanlines
			(
				g_pReorderedFramebufferbits,
				g_pFramebufferbits,
				g_pFramebufferinfo->bmiHeader.biWidth,
				g_pFramebufferinfo->bmiHeader.biHeight,
				g_pFramebufferinfo->bmiHeader.biBitCount / 8
			);
		}

		CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
		bool bWantMouse = (pMouseCard && pMouseCard->IsActiveAndEnabled());
		// TODO: only send the framebuffer out when not in trackonly_mode
		GameLink::Out(
			(UINT16)g_pFramebufferinfo->bmiHeader.biWidth,
			(UINT16)g_pFramebufferinfo->bmiHeader.biHeight,
			1.0,								// image ratio
			bWantMouse,
			(const UINT8*)g_pReorderedFramebufferbits,
			MemGetBankPtr(0));					// Main memory pointer
	}
		/////////////////////////////////////////////////////////////////////////////////////////
		// Now get the input from GameLink
		// and fire off all necessary inputs to Applewin
		// TODO: Put this at the beginning of the main loop to ensure we're not 1 frame behind
		// TODO: Handle Gamelink track-only

	if (GameLink::GetGameLinkEnabled() && GameLink::In(&g_gamelink.input, &g_gamelink.audio)) {
		// -- Audio input
		SpkrSetVolume(g_gamelink.audio.master_vol_l, 100);
		MB_SetVolume(g_gamelink.audio.master_vol_l, 100);

		// -- Mouse input
		//g_gamelink.input.mouse_dx
		// The Gamelink mouse input gives us delta x and delta y
		// The windows PostMessage WM_MOUSEMOVE requires absolute positioning on the frame window
		// We could go straight to the MouseInterface but using PostMessage is more WYSIWYG
		CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
		bool bWantMouse = (pMouseCard && pMouseCard->IsActiveAndEnabled());
		if (bWantMouse) {
			int iX, iMinX, iMaxX, iY, iMinY, iMaxY;
			pMouseCard->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);
			// Get absolute coordinates
			int iMouseX = iX + (int)g_gamelink.input.mouse_dx;
			int iMouseY = iY + (int)g_gamelink.input.mouse_dy;
			// The lparam has Y as high byte and X as low byte
			LPARAM lparam = ((iMouseY << 8) | iMouseX & 0x00FF);
			// Check button presses for WPARAM
			WPARAM wparam = 0;
			// Cache old and new
			const UINT8 old = g_gamelink.input_prev.mouse_btn;
			const UINT8 btn = g_gamelink.input.mouse_btn;

			// Mouse buttons are LEFT, RIGHT, MIDDLE
			// TODO: Check also for CONTROL and SHIFT here?
			for (UINT8 i = 0; i < 3; ++i)
			{
				const UINT8 mask = 1 << i;
				if ((btn & mask) && !(old & mask)) {
					if (i < 1)
						wparam |= MK_LBUTTON;
					else if (i < 2)
						wparam |= MK_RBUTTON;
					else
						wparam |= MK_MBUTTON;
				}
			}
			LogOutput("Mouse X,Y, WPARAM: %02d %02d %04X\n", g_gamelink.input.mouse_dx, g_gamelink.input.mouse_dy, wparam);
			PostMessageW(g_hFrameWindow, WM_MOUSEMOVE, wparam, lparam);
		}


		// -- Keyboard input
		// Gamelink sets in shm 8 UINT32s, for a total of $FF bits
		// Which match the space of keyboard scancodes
		// Each bit will state if the scancode at that position is pressed (1) or released (0)
		// We keep a cache of the previous state, so we'll know if a key has changed state
		// and trigger the event
		std::string sKeybString = "00000409";
		HKL hKeyboardLayout = GetKeyboardLayout(0);
//		HKL hKeyboardLayout = LoadKeyboardLayoutW((LPCWSTR)"00000409", KLF_SUBSTITUTE_OK);	// TODO put this in a file global

//		LPBYTE pKeyboardState = new UINT8[256];
		UINT8 aDIKtoVK[256] = { 0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0xBD, 0xBB,
								0x08, 0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0xDB, 0xDD, 0x0D,
								0xA2, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0xBA, 0xDE, 0xC0, 0xA0, 0xDC,
								0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0xBC, 0xBE, 0xBF, 0xA1, 0x6A, 0xA4, 0x20, 0x14,
								0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x90, 0x91, 0x24, 0x26, 0x21,
								0x6D, 0x25, 0x0C, 0x27, 0x6B, 0x23, 0x28, 0x22, 0x2D, 0x2E, 0x2C, 0x00, 0xE2, 0x7A, 0x7B,
								0x0C, 0xEE, 0xF1, 0xEA, 0xF9, 0xF5, 0xF3, 0x00, 0x00, 0xFB, 0x2F, 0x7C, 0x7D, 0x7E, 0x7F,
								0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0xED, 0x00, 0xE9, 0x00, 0xC1, 0x00, 0x00, 0x87,
								0x00, 0x00, 0x00, 0x00, 0xEB, 0x09, 0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB1, 0x00, 0x00, 0x00, 0x00,
								0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x0D, 0xA3, 0x00, 0x00, 0xAD, 0xB6, 0xB3, 0x00,
								0xB2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAE, 0x00, 0xAF, 0x00, 0xB7,
								0x00, 0x00, 0xBF, 0x00, 0x2A, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								0x00, 0x00, 0x00, 0x90, 0x00, 0x24, 0x26, 0x21, 0x00, 0x25, 0x00, 0x27, 0x00, 0x23, 0x28,
								0x22, 0x2D, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x5C, 0x5D, 0x00, 0x5F,
								0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xAB, 0xA8, 0xA9, 0xA7, 0xA6, 0xAC, 0xB4, 0xB5, 0x00,
								0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
								0x00, 0x00 };

		for (UINT8 blk = 0; blk < 8; ++blk)
		{
			const UINT old = g_gamelink.input_prev.keyb_state[blk];
			const UINT key = g_gamelink.input.keyb_state[blk];
			UINT8 scancode;
			UINT32 mask;
			UINT iKeyState;
			UINT iVK_Code;

			for (UINT8 bit = 0; bit < 32; ++bit)
			{
				iKeyState = 0;
				scancode = static_cast<UINT8>((blk * 32) + bit);
				mask = 1 << bit;
				if ((key & mask) && !(old & mask)) {
					iKeyState = WM_KEYDOWN;
//					pKeyboardState[scancode] = 0b10000001;
//					iVK_Code = MapVirtualKeyExA(scancode, MAPVK_VSC_TO_VK_EX, hKeyboardLayout);
//					KeybQueueKeypress(iVK_Code, NOT_ASCII);
//					KeybAnyKeyDown(iKeyState, iVK_Code, 0);
				}
				if (!(key & mask) && (old & mask)) {
					iKeyState = WM_KEYUP;
//					pKeyboardState[scancode] = 0;
//					iVK_Code = MapVirtualKeyExA(scancode, MAPVK_VSC_TO_VK_EX, hKeyboardLayout);
//					KeybQueueKeypress(iVK_Code, NOT_ASCII);
//					KeybAnyKeyDown(iKeyState, iVK_Code, 0);
				}
				if (iKeyState)
				{
//					SetKeyboardState(pKeyboardState);
					iVK_Code = aDIKtoVK[scancode];
//					iVK_Code = MapVirtualKeyExW(scancode, MAPVK_VSC_TO_VK_EX, hKeyboardLayout);
//					if (iKeyState == WM_KEYUP)
//   						keybd_event(iVK_Code, scancode, KEYEVENTF_KEYUP, 0);	// doesn't work
//					else
//						keybd_event(iVK_Code, scancode, 0, 0);	// doesn't work
					// https://stackoverflow.com/questions/10280000/how-to-create-lparam-of-sendmessage-wm-keydown#10281086
					// Build the generic lparam to be used for WM_KEYDOWN/WM_KEYUP/WM_CHAR
					LPARAM lparam = 0x00000001 | (LPARAM)(scancode << 16);         // Scan code, repeat=1
					if (scancode > 0x7F)
						lparam = lparam | 0x01000000;	// set extended
					//FrameWndProc(g_hFrameWindow, iKeyState, iVK_Code, lparam);
					PostMessageW(g_hFrameWindow, iKeyState, iVK_Code, lparam);
//					LogOutput("SCANCODE: %04X\n", scancode);
//					LogOutput("iVK: %04X\n", iVK_Code);
//					LogOutput("LPARAM: %04X\n", lparam);
					// the directions key seem to be sending 0 as the scancode, at least in Frame.cpp's KeybQueueKeypress
					// And although regular keys are triggering KeybQueueKeypress correctly, nothing is happening on the screen.
				}
//				pKeyboardState++;
			}
		}
		// We're done parsing the input. Store it as the previous state
		memcpy(&g_gamelink.input_prev, &g_gamelink.input, sizeof(GameLink::sSharedMMapInput_R2));
//		delete [] pKeyboardState;
	}
	// RIK END

#ifdef NO_DIRECT_X
#else
	//if (g_lpDD) g_lpDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
#endif // NO_DIRECT_X

	GdiFlush();
}

//===========================================================================
void VideoReinitialize (bool bInitVideoScannerAddress /*= true*/)
{
	NTSC_VideoReinitialize( g_dwCyclesThisFrame, bInitVideoScannerAddress );
	NTSC_VideoInitAppleType();
	NTSC_SetVideoStyle();
	NTSC_SetVideoTextMode( g_uVideoMode &  VF_80COL ? 80 : 40 );
	NTSC_SetVideoMode( g_uVideoMode );	// Pre-condition: g_nVideoClockHorz (derived from g_dwCyclesThisFrame)
	VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());
}

//===========================================================================
void VideoResetState ()
{
	g_nAltCharSetOffset    = 0;
	g_uVideoMode           = VF_TEXT;

	NTSC_SetVideoTextMode( 40 );
	NTSC_SetVideoMode( g_uVideoMode );

	RGB_ResetState();
}

//===========================================================================

BYTE VideoSetMode(WORD, WORD address, BYTE write, BYTE, ULONG uExecutedCycles)
{
	address &= 0xFF;

	const uint32_t oldVideoMode = g_uVideoMode;

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

	if (!IS_APPLE2)
		RGB_SetVideoMode(address);

	// Only 1-cycle delay for VF_TEXT & VF_MIXED mode changes (GH#656)
	bool delay = false;
	if ((oldVideoMode ^ g_uVideoMode) & (VF_TEXT|VF_MIXED))
		delay = true;

	NTSC_SetVideoMode( g_uVideoMode, delay );

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

#define SS_YAML_KEY_ALT_CHARSET "Alt Char Set"
#define SS_YAML_KEY_VIDEO_MODE "Video Mode"
#define SS_YAML_KEY_CYCLES_THIS_FRAME "Cycles This Frame"
#define SS_YAML_KEY_VIDEO_REFRESH_RATE "Video Refresh Rate"

static std::string VideoGetSnapshotStructName(void)
{
	static const std::string name("Video");
	return name;
}

void VideoSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", VideoGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveBool(SS_YAML_KEY_ALT_CHARSET, g_nAltCharSetOffset ? true : false);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_VIDEO_MODE, g_uVideoMode);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_CYCLES_THIS_FRAME, g_dwCyclesThisFrame);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_VIDEO_REFRESH_RATE, (UINT)GetVideoRefreshRate());
}

void VideoLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (!yamlLoadHelper.GetSubMap(VideoGetSnapshotStructName()))
		return;

	if (version >= 4)
	{
		VideoRefreshRate_e rate = (VideoRefreshRate_e)yamlLoadHelper.LoadUint(SS_YAML_KEY_VIDEO_REFRESH_RATE);
		SetVideoRefreshRate(rate);	// Trashes: g_dwCyclesThisFrame
		SetCurrentCLK6502();
	}

	g_nAltCharSetOffset = yamlLoadHelper.LoadBool(SS_YAML_KEY_ALT_CHARSET) ? 256 : 0;
	g_uVideoMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_VIDEO_MODE);
	g_dwCyclesThisFrame = yamlLoadHelper.LoadUint(SS_YAML_KEY_CYCLES_THIS_FRAME);

	yamlLoadHelper.PopMap();
}

//===========================================================================
//
// References to Jim Sather's books are given as eg:
// UTAIIe:5-7,P3 (Understanding the Apple IIe, chapter 5, page 7, Paragraph 3)
//
WORD VideoGetScannerAddress(DWORD nCycles, VideoScanner_e videoScannerAddr /*= VS_FullAddr*/)
{
    // machine state switches
    //
    bool bHires   = VideoGetSWHIRES() && !VideoGetSWTEXT();
    bool bPage2   = VideoGetSWPAGE2();
    bool b80Store = VideoGetSW80STORE();

    // calculate video parameters according to display standard
    //
    const int kScanLines  = g_bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
    const int kScanCycles = kScanLines * kHClocks;
    _ASSERT(nCycles < (UINT)kScanCycles);
    nCycles %= kScanCycles;

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

    // calculate vertical scanning state (UTAIIe:3-15,T3.2)
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if (nVLine >= kVPresetLine) // check for previous vertical state preset
    {
        nVState -= kScanLines; // compensate for preset
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
    if (bHires && SW_MIXED && v_4 && v_2) // HIRES TIME signal (UTAIIe:5-7,P3)
    {
        bHires = false; // address is in text memory for mixed hires
    }

    int nAddend0 = 0x0D; // 1            1            0            1
    int nAddend1 =              (h_5 << 2) | (h_4 << 1) | (h_3 << 0);
    int nAddend2 = (v_4 << 3) | (v_3 << 2) | (v_4 << 1) | (v_3 << 0);
    int nSum     = (nAddend0 + nAddend1 + nAddend2) & 0x0F; // SUM (UTAIIe:5-9)

    WORD nAddressH = 0; // build address from video scanner equations (UTAIIe:5-8,T5.1)
    nAddressH |= h_0  << 0; // a0
    nAddressH |= h_1  << 1; // a1
    nAddressH |= h_2  << 2; // a2
    nAddressH |= nSum << 3; // a3 - a6
    if (!bHires)
    {
        // Apple ][ (not //e) and HBL?
        //
        if (IS_APPLE2 && // Apple II only (UTAIIe:I-4,#5)
            !h_5 && (!h_4 || !h_3)) // HBL (UTAIIe:8-10,F8.5)
        {
            nAddressH |= 1 << 12; // Y: a12 (add $1000 to address!)
        }
    }

    WORD nAddressV = 0;
    nAddressV |= v_0  << 7; // a7
    nAddressV |= v_1  << 8; // a8
    nAddressV |= v_2  << 9; // a9

    int p2a = !(bPage2 && !b80Store) ? 1 : 0;
    int p2b =  (bPage2 && !b80Store) ? 1 : 0;

    WORD nAddressP = 0;	// Page bits
    if (bHires) // hires?
    {
        // Y: insert hires-only address bits
        //
        nAddressV |= v_A << 10; // a10
        nAddressV |= v_B << 11; // a11
        nAddressV |= v_C << 12; // a12
        nAddressP |= p2a << 13; // a13
        nAddressP |= p2b << 14; // a14
    }
    else
    {
        // N: insert text-only address bits
        //
        nAddressP |= p2a << 10; // a10
        nAddressP |= p2b << 11; // a11
	}

	// VBL' = v_4' | v_3' = (v_4 & v_3)' (UTAIIe:5-10,#3),  (UTAIIe:3-15,T3.2)

	if (videoScannerAddr == VS_PartialAddrH)
		return nAddressH;

	if (videoScannerAddr == VS_PartialAddrV)
		return nAddressV;

    return nAddressP | nAddressV | nAddressH;
}

//===========================================================================

// Called when *outside* of CpuExecute()
bool VideoGetVblBarEx(const DWORD dwCyclesThisFrame)
{
	if (g_bFullSpeed)
	{
		// Ensure that NTSC video-scanner gets updated during full-speed, so video screen can be redrawn during Apple II VBL
		NTSC_VideoClockResync(dwCyclesThisFrame);
	}

	return g_nVideoClockVert < kVDisplayableScanLines;
}

// Called when *inside* CpuExecute()
bool VideoGetVblBar(const DWORD uExecutedCycles)
{
	if (g_bFullSpeed)
	{
		// Ensure that NTSC video-scanner gets updated during full-speed, so video-dependent Apple II code doesn't hang
		NTSC_VideoClockResync(CpuGetCyclesThisVideoFrame(uExecutedCycles));
	}

	return g_nVideoClockVert < kVDisplayableScanLines;
}

//===========================================================================

#define MAX_DRAW_DEVICES 10

static char *draw_devices[MAX_DRAW_DEVICES];
static GUID draw_device_guid[MAX_DRAW_DEVICES];
static int num_draw_devices = 0;

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

#define SCREENSHOT_BMP 1
#define SCREENSHOT_TGA 0
	
static int  g_nLastScreenShot = 0;
const  int nMaxScreenShot = 999999999;
static std::string g_pLastDiskImageName;

//===========================================================================
void Video_ResetScreenshotCounter( const std::string & pImageName )
{
	g_nLastScreenShot = 0;
	g_pLastDiskImageName = pImageName;
}

//===========================================================================
void Util_MakeScreenShotFileName( TCHAR *pFinalFileName_, DWORD chars )
{
	const std::string sPrefixScreenShotFileName = "AppleWin_ScreenShot";
	// TODO: g_sScreenshotDir
	const std::string pPrefixFileName = !g_pLastDiskImageName.empty() ? g_pLastDiskImageName : sPrefixScreenShotFileName;
#if SCREENSHOT_BMP
	StringCbPrintf( pFinalFileName_, chars, TEXT("%s_%09d.bmp"), pPrefixFileName.c_str(), g_nLastScreenShot );
#endif
#if SCREENSHOT_TGA
	StringCbPrintf( pFinalFileName_, chars, TEXT("%s%09d.tga"), pPrefixFileName.c_str(), g_nLastScreenShot );
#endif
}

// Returns TRUE if file exists, else FALSE
//===========================================================================
bool Util_TestScreenShotFileName( const TCHAR *pFileName )
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
	TCHAR sScreenShotFileName[ MAX_PATH ];

	// find last screenshot filename so we don't overwrite the existing user ones
	bool bExists = true;
	while( bExists )
	{
		if (g_nLastScreenShot > nMaxScreenShot) // Holy Crap! User has maxed the number of screenshots!?
		{
			TCHAR msg[512];
			StringCbPrintf( msg, 512, "You have more then %d screenshot filenames!  They will no longer be saved.\n\nEither move some of your screenshots or increase the maximum in video.cpp\n", nMaxScreenShot );
			MessageBox( g_hFrameWindow, msg, "Warning", MB_OK );
			g_nLastScreenShot = 0;
			return;
		}

		Util_MakeScreenShotFileName( sScreenShotFileName, MAX_PATH );
		bExists = Util_TestScreenShotFileName( sScreenShotFileName );
		if( !bExists )
		{
			break;
		}
		g_nLastScreenShot++;
	}

	Video_SaveScreenShot( ScreenShotType, sScreenShotFileName );
	g_nLastScreenShot++;
}

void Video_RedrawAndTakeScreenShot( const TCHAR* pScreenshotFilename )
{
	_ASSERT(pScreenshotFilename);
	if (!pScreenshotFilename)
		return;

	VideoRedrawScreen();
	Video_SaveScreenShot( SCREENSHOT_560x384, pScreenshotFilename );
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
static void Video_SaveScreenShot( const VideoScreenShot_e ScreenShotType, const TCHAR *pScreenShotFileName )
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

static const UINT kVideoRomSize8K = kVideoRomSize4K*2;
static const UINT kVideoRomSize16K = kVideoRomSize8K*2;
static const UINT kVideoRomSizeMax = kVideoRomSize16K;
static BYTE g_videoRom[kVideoRomSizeMax];
static UINT g_videoRomSize = 0;
static bool g_videoRomRockerSwitch = false;

bool ReadVideoRomFile(const TCHAR* pRomFile)
{
	g_videoRomSize = 0;

	HANDLE h = CreateFile(pRomFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	const ULONG size = GetFileSize(h, NULL);
	if (size == kVideoRomSize2K || size == kVideoRomSize4K || size == kVideoRomSize8K || size == kVideoRomSize16K)
	{
		DWORD bytesRead;
		if (ReadFile(h, g_videoRom, size, &bytesRead, NULL) && bytesRead == size)
			g_videoRomSize = size;
	}

	if (g_videoRomSize == kVideoRomSize16K)
	{
		// Use top 8K (assume bottom 8K is all 0xFF's)
		memcpy(&g_videoRom[0], &g_videoRom[kVideoRomSize8K], kVideoRomSize8K);
		g_videoRomSize = kVideoRomSize8K;
	}

	CloseHandle(h);

	return g_videoRomSize != 0;
}

UINT GetVideoRom(const BYTE*& pVideoRom)
{
	pVideoRom = &g_videoRom[0];
	return g_videoRomSize;
}

bool GetVideoRomRockerSwitch(void)
{
	return g_videoRomRockerSwitch;
}

void SetVideoRomRockerSwitch(bool state)
{
	g_videoRomRockerSwitch = state;
}

bool IsVideoRom4K(void)
{
	return g_videoRomSize <= kVideoRomSize4K;
}

//===========================================================================

enum VideoType127_e
{
	  VT127_MONO_CUSTOM
	, VT127_COLOR_MONITOR_NTSC
	, VT127_MONO_TV
	, VT127_COLOR_TV
	, VT127_MONO_AMBER
	, VT127_MONO_GREEN
	, VT127_MONO_WHITE
	, VT127_NUM_VIDEO_MODES
};

void Config_Load_Video()
{
	DWORD dwTmp;

	REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_MODE), &dwTmp, (DWORD)VT_DEFAULT);
	g_eVideoType = dwTmp;

	REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_STYLE), &dwTmp, (DWORD)VS_HALF_SCANLINES);
	g_eVideoStyle = (VideoStyle_e)dwTmp;

	REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_MONO_COLOR), &dwTmp, (DWORD)RGB(0xC0, 0xC0, 0xC0));
	g_nMonochromeRGB = (COLORREF)dwTmp;

	REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_REFRESH_RATE), &dwTmp, (DWORD)VR_60HZ);
	SetVideoRefreshRate((VideoRefreshRate_e)dwTmp);

	// RIK START
	REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_GAMELINK), &dwTmp, (DWORD)false);
	GameLink::SetGameLinkEnabled(dwTmp);
	// RIK END


	//

	const UINT16* pOldVersion = GetOldAppleWinVersion();
	if (pOldVersion[0] == 1 && pOldVersion[1] <= 28 && pOldVersion[2] <= 1)
	{
		DWORD dwHalfScanLines;
		REGLOAD_DEFAULT(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES), &dwHalfScanLines, 0);

		if (dwHalfScanLines)
			g_eVideoStyle = (VideoStyle_e) ((DWORD)g_eVideoStyle | VS_HALF_SCANLINES);
		else
			g_eVideoStyle = (VideoStyle_e) ((DWORD)g_eVideoStyle & ~VS_HALF_SCANLINES);

		REGSAVE(TEXT(REGVALUE_VIDEO_STYLE), g_eVideoStyle);
	}

	//

	if (pOldVersion[0] == 1 && pOldVersion[1] <= 27 && pOldVersion[2] <= 13)
	{
		switch (g_eVideoType)
		{
		case VT127_MONO_CUSTOM:			g_eVideoType = VT_MONO_CUSTOM; break;
		case VT127_COLOR_MONITOR_NTSC:	g_eVideoType = VT_COLOR_MONITOR_NTSC; break;
		case VT127_MONO_TV:				g_eVideoType = VT_MONO_TV; break;
		case VT127_COLOR_TV:			g_eVideoType = VT_COLOR_TV; break;
		case VT127_MONO_AMBER:			g_eVideoType = VT_MONO_AMBER; break;
		case VT127_MONO_GREEN:			g_eVideoType = VT_MONO_GREEN; break;
		case VT127_MONO_WHITE:			g_eVideoType = VT_MONO_WHITE; break;
		default:						g_eVideoType = VT_DEFAULT; break;
		}

		REGSAVE(TEXT(REGVALUE_VIDEO_MODE), g_eVideoType);
	}

	if (g_eVideoType >= NUM_VIDEO_MODES)
		g_eVideoType = VT_DEFAULT;
}

void Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE)      ,g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_STYLE)     ,g_eVideoStyle);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR),g_nMonochromeRGB);
	REGSAVE(TEXT(REGVALUE_VIDEO_REFRESH_RATE), GetVideoRefreshRate());
	REGSAVE(TEXT(REGVALUE_VIDEO_GAMELINK), GameLink::GetGameLinkEnabled());		// RIK
}

//===========================================================================

VideoType_e GetVideoType(void)
{
	return (VideoType_e) g_eVideoType;
}

// TODO: Can only do this at start-up (mid-emulation requires a more heavy-weight video reinit)
void SetVideoType(VideoType_e newVideoType)
{
	g_eVideoType = newVideoType;
}

VideoStyle_e GetVideoStyle(void)
{
	return g_eVideoStyle;
}

void SetVideoStyle(VideoStyle_e newVideoStyle)
{
	g_eVideoStyle = newVideoStyle;
}

bool IsVideoStyle(VideoStyle_e mask)
{
	return (g_eVideoStyle & mask) != 0;
}

//===========================================================================

VideoRefreshRate_e GetVideoRefreshRate(void)
{
	return (g_bVideoScannerNTSC == false) ? VR_50HZ : VR_60HZ;
}

void SetVideoRefreshRate(VideoRefreshRate_e rate)
{
	if (rate != VR_50HZ)
		rate = VR_60HZ;

	g_bVideoScannerNTSC = (rate == VR_60HZ);
	NTSC_SetRefreshRate(rate);
}

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

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	UINT fbSize = GetFrameBufferWidth() * GetFrameBufferHeight() * sizeof(bgra_t);
	ZeroMemory(g_pFramebufferbits, fbSize);
	ZeroMemory(g_pReorderedFramebufferbits, fbSize);			// RIK

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	NTSC_VideoInit( g_pFramebufferbits );
}

//===========================================================================

const char* VideoGetAppWindowTitle(void)
{
	static const char *apVideoMonitorModeDesc[ 2 ] =
	{
		"Color (NTSC Monitor)",
		"Color (PAL Monitor)"
	};

	const VideoType_e videoType = GetVideoType();
	if ( videoType != VT_COLOR_MONITOR_NTSC)
		return g_apVideoModeDesc[ videoType ];
	else
		return apVideoMonitorModeDesc[ GetVideoRefreshRate() == VR_60HZ ? 0 : 1 ];	// NTSC or PAL
}
