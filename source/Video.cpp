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

#include "Video.h"
#include "Core.h"
#include "CPU.h"
#include "Interface.h"
#include "Log.h"
#include "Memory.h"
#include "Registry.h"
#include "NTSC.h"
#include "RGBMonitor.h"

#include "YamlHelper.h"

	#define  SW_80COL         (g_uVideoMode & VF_80COL)
	#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
	#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
	#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
	#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
	#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
	#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

// Globals (Public)

    uint8_t      *g_pFramebufferbits = NULL; // last drawn frame (initialized in WinVideoInitialize)
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

COLORREF         g_nMonochromeRGB    = RGB(0xC0,0xC0,0xC0);

uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

DWORD     g_eVideoType     = VT_DEFAULT;
static VideoStyle_e g_eVideoStyle = VS_HALF_SCANLINES;

static bool g_bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

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
	static const char *g_apVideoModeDesc[ NUM_VIDEO_MODES ] =
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
	static void Util_MakeScreenShotFileName( TCHAR *pFinalFileName_, DWORD chars );
	static bool Util_TestScreenShotFileName( const TCHAR *pFileName );
	static void Video_MakeScreenShot( FILE *pFile, const VideoScreenShot_e ScreenShotType );
	static void videoCreateDIBSection();

//===========================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

UINT GetFrameBufferBorderlessWidth(void)
{
	static const UINT uFrameBufferBorderlessW = 560;	// 560 = Double Hi-Res
	return uFrameBufferBorderlessW;
}

UINT GetFrameBufferBorderlessHeight(void)
{
	static const UINT uFrameBufferBorderlessH = 384;	// 384 = Double Scan Line
	return uFrameBufferBorderlessH;
}

// NB. These border areas are not visible (... and these border areas are unrelated to the 3D border below)
UINT GetFrameBufferBorderWidth(void)
{
	static const UINT uBorderW = 20;
	return uBorderW;
}

UINT GetFrameBufferBorderHeight(void)
{
	static const UINT uBorderH = 18;
	return uBorderH;
}

UINT GetFrameBufferWidth(void)
{
	return GetFrameBufferBorderlessWidth() + 2 * GetFrameBufferBorderWidth();
}

UINT GetFrameBufferHeight(void)
{
	return GetFrameBufferBorderlessHeight() + 2 * GetFrameBufferBorderHeight();
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
			MessageBox( GetFrame().g_hFrameWindow, msg, "Warning", MB_OK );
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
//	MessageBox( GetFrame().g_hFrameWindow, sText, "Info 1", MB_OK );
//	sprintf( sText, "sizeof: BITMAPINFOHEADER = %d\n", sizeof(BITMAPINFOHEADER) ); // = 40
//	MessageBox( GetFrame().g_hFrameWindow, sText, "Info 2", MB_OK );

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
void Video_SaveScreenShot( const VideoScreenShot_e ScreenShotType, const TCHAR *pScreenShotFileName )
{
	FILE *pFile = fopen( pScreenShotFileName, "wb" );
	if( pFile )
	{
		Video_MakeScreenShot( pFile, ScreenShotType );
		fclose( pFile );
	}

	if( g_bDisplayPrintScreenFileName )
	{
		MessageBox( GetFrame().g_hFrameWindow, pScreenShotFileName, "Screen Captured", MB_OK );
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
