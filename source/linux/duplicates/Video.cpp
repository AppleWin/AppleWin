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

#include "Memory.h"
#include "Registry.h"
#include "Video.h"
#include "NTSC.h"
#include "RGBMonitor.h"
#include "Frame.h"
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

static LPBITMAPINFO  g_pFramebufferinfo = NULL;

COLORREF         g_nMonochromeRGB    = RGB(0xC0,0xC0,0xC0);

uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

DWORD     g_eVideoType     = VT_DEFAULT;
static VideoStyle_e g_eVideoStyle = VS_HALF_SCANLINES;

static bool g_bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

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

static void videoCreateDIBSection();


void VideoInitialize ()
{
	// RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
	VideoResetState();

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


void VideoDestroy () {

  // DESTROY BUFFERS
  VirtualFree(g_pFramebufferinfo,0,MEM_RELEASE);
  g_pFramebufferinfo = NULL;
  free(g_pFramebufferbits);
  g_pFramebufferbits = NULL;
}


void VideoReinitialize (bool bInitVideoScannerAddress /*= true*/)
{
	NTSC_VideoReinitialize( g_dwCyclesThisFrame, bInitVideoScannerAddress );
	NTSC_VideoInitAppleType();
	NTSC_SetVideoStyle();
	NTSC_SetVideoTextMode( g_uVideoMode &  VF_80COL ? 80 : 40 );
	NTSC_SetVideoMode( g_uVideoMode );	// Pre-condition: g_nVideoClockHorz (derived from g_dwCyclesThisFrame)
}

void VideoResetState ()
{
	g_nAltCharSetOffset    = 0;
	g_uVideoMode           = VF_TEXT;

	NTSC_SetVideoTextMode( 40 );
	NTSC_SetVideoMode( g_uVideoMode );

	RGB_ResetState();
}

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

	bool delay = true;
	if ((oldVideoMode ^ g_uVideoMode) & VF_PAGE2)
		delay = false;	// PAGE2 flag changed state, so no 1 cycle delay (GH#656)

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

	if (g_eVideoType >= NUM_VIDEO_MODES)
		g_eVideoType = VT_DEFAULT;
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

void Video_ResetScreenshotCounter( const std::string & pImageName )
{
}

void VideoRedrawScreen (void)
{
	// NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
	VideoRefreshScreen( g_uVideoMode, true );
}

void VideoRefreshScreen ( uint32_t uRedrawWholeScreenVideoMode /* =0*/, bool bRedrawWholeScreen /* =false*/ )
{
	if (bRedrawWholeScreen)
	{
		// uVideoModeForWholeScreen set if:
		// . MODE_DEBUG   : always
		// . MODE_RUNNING : called from VideoRedrawScreen(), eg. during full-speed
		if (bRedrawWholeScreen)
			NTSC_SetVideoMode( uRedrawWholeScreenVideoMode );
		NTSC_VideoRedrawWholeScreen();
	}
}

static void videoCreateDIBSection()
{
        const size_t size = GetFrameBufferWidth()*GetFrameBufferHeight()*sizeof(bgra_t);
	void * memory = malloc(size);
        g_pFramebufferbits = static_cast<uint8_t *>(memory);

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	ZeroMemory( g_pFramebufferbits, size);

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	NTSC_VideoInit( g_pFramebufferbits );
}

void getScreenData(uint8_t * & data, int & width, int & height, int & sx, int & sy, int & sw, int & sh)
{
  data = g_pFramebufferbits;
  width = GetFrameBufferWidth();
  height = GetFrameBufferHeight();
  sx = GetFrameBufferBorderWidth();
  sy = GetFrameBufferBorderHeight();
  sw = GetFrameBufferBorderlessWidth();
  sh = GetFrameBufferBorderlessHeight();
}

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

void Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE)      ,g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_STYLE)     ,g_eVideoStyle);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR),g_nMonochromeRGB);
	REGSAVE(TEXT(REGVALUE_VIDEO_REFRESH_RATE), GetVideoRefreshRate());
}
