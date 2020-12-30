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

//-------------------------------------

// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
const char Video::g_aVideoChoices[] =
	"Monochrome (Custom)\0"
	"Color (Composite Idealized)\0"
	"Color (RGB Card/Monitor)\0"
	"Color (Composite Monitor)\0"
	"Color TV\0"
	"B&W TV\0"
	"Monochrome (Amber)\0"
	"Monochrome (Green)\0"
	"Monochrome (White)\0"
	;

// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
// The window title will be set to this.

const char Video::m_szModeDesc0[] = "Monochrome (Custom)";
const char Video::m_szModeDesc1[] = "Color (Composite Idealized)";
const char Video::m_szModeDesc2[] = "Color (RGB Card/Monitor)";
const char Video::m_szModeDesc3[] = "Color (Composite Monitor)";
const char Video::m_szModeDesc4[] = "Color TV";
const char Video::m_szModeDesc5[] = "B&W TV";
const char Video::m_szModeDesc6[] = "Monochrome (Amber)";
const char Video::m_szModeDesc7[] = "Monochrome (Green)";
const char Video::m_szModeDesc8[] = "Monochrome (White)";

const char* const Video::g_apVideoModeDesc[NUM_VIDEO_MODES] =
{
	Video::m_szModeDesc0,
	Video::m_szModeDesc1,
	Video::m_szModeDesc2,
	Video::m_szModeDesc3,
	Video::m_szModeDesc4,
	Video::m_szModeDesc5,
	Video::m_szModeDesc6,
	Video::m_szModeDesc7,
	Video::m_szModeDesc8
};

//===========================================================================

UINT Video::GetFrameBufferBorderlessWidth(void)
{
	static const UINT uFrameBufferBorderlessW = 560;	// 560 = Double Hi-Res
	return uFrameBufferBorderlessW;
}

UINT Video::GetFrameBufferBorderlessHeight(void)
{
	static const UINT uFrameBufferBorderlessH = 384;	// 384 = Double Scan Line
	return uFrameBufferBorderlessH;
}

// NB. These border areas are not visible (... and these border areas are unrelated to the 3D border below)
UINT Video::GetFrameBufferBorderWidth(void)
{
	static const UINT uBorderW = 20;
	return uBorderW;
}

UINT Video::GetFrameBufferBorderHeight(void)
{
	static const UINT uBorderH = 18;
	return uBorderH;
}

UINT Video::GetFrameBufferWidth(void)
{
	return GetFrameBufferBorderlessWidth() + 2 * GetFrameBufferBorderWidth();
}

UINT Video::GetFrameBufferHeight(void)
{
	return GetFrameBufferBorderlessHeight() + 2 * GetFrameBufferBorderHeight();
}

//===========================================================================

void Video::VideoReinitialize(bool bInitVideoScannerAddress /*= true*/)
{
	NTSC_VideoReinitialize( g_dwCyclesThisFrame, bInitVideoScannerAddress );
	NTSC_VideoInitAppleType();
	NTSC_SetVideoStyle();
	NTSC_SetVideoTextMode( g_uVideoMode &  VF_80COL ? 80 : 40 );
	NTSC_SetVideoMode( g_uVideoMode );	// Pre-condition: g_nVideoClockHorz (derived from g_dwCyclesThisFrame)
	VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());
}

//===========================================================================

void Video::VideoResetState(void)
{
	g_nAltCharSetOffset    = 0;
	g_uVideoMode           = VF_TEXT;

	NTSC_SetVideoTextMode( 40 );
	NTSC_SetVideoMode( g_uVideoMode );

	RGB_ResetState();
}

//===========================================================================

BYTE Video::VideoSetMode(WORD, WORD address, BYTE write, BYTE, ULONG uExecutedCycles)
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

bool Video::VideoGetSW80COL(void)
{
	return SW_80COL ? true : false;
}

bool Video::VideoGetSWDHIRES(void)
{
	return SW_DHIRES ? true : false;
}

bool Video::VideoGetSWHIRES(void)
{
	return SW_HIRES ? true : false;
}

bool Video::VideoGetSW80STORE(void)
{
	return SW_80STORE ? true : false;
}

bool Video::VideoGetSWMIXED(void)
{
	return SW_MIXED ? true : false;
}

bool Video::VideoGetSWPAGE2(void)
{
	return SW_PAGE2 ? true : false;
}

bool Video::VideoGetSWTEXT(void)
{
	return SW_TEXT ? true : false;
}

bool Video::VideoGetSWAltCharSet(void)
{
	return g_nAltCharSetOffset != 0;
}

//===========================================================================

#define SS_YAML_KEY_ALT_CHARSET "Alt Char Set"
#define SS_YAML_KEY_VIDEO_MODE "Video Mode"
#define SS_YAML_KEY_CYCLES_THIS_FRAME "Cycles This Frame"
#define SS_YAML_KEY_VIDEO_REFRESH_RATE "Video Refresh Rate"

std::string Video::VideoGetSnapshotStructName(void)
{
	static const std::string name("Video");
	return name;
}

void Video::VideoSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", VideoGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveBool(SS_YAML_KEY_ALT_CHARSET, g_nAltCharSetOffset ? true : false);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_VIDEO_MODE, g_uVideoMode);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_CYCLES_THIS_FRAME, g_dwCyclesThisFrame);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_VIDEO_REFRESH_RATE, (UINT)GetVideoRefreshRate());
}

void Video::VideoLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
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
WORD Video::VideoGetScannerAddress(DWORD nCycles, VideoScanner_e videoScannerAddr /*= VS_FullAddr*/)
{
	const int kHBurstClock      =    53; // clock when Color Burst starts
	const int kHBurstClocks     =     4; // clocks per Color Burst duration
	const int kHClock0State     =  0x18; // H[543210] = 011000
	const int kHClocks          =    65; // clocks per horizontal scan (including HBL)
	const int kHPEClock         =    40; // clock when HPE (horizontal preset enable) goes low
	const int kHPresetClock     =    41; // clock when H state presets
	const int kHSyncClock       =    49; // clock when HSync starts
	const int kHSyncClocks      =     4; // clocks per HSync duration
	const int kNTSCScanLines    =   262; // total scan lines including VBL (NTSC)
	const int kNTSCVSyncLine    =   224; // line when VSync starts (NTSC)
	const int kPALScanLines     =   312; // total scan lines including VBL (PAL)
	const int kPALVSyncLine     =   264; // line when VSync starts (PAL)
	const int kVLine0State      = 0x100; // V[543210CBA] = 100000000
	const int kVPresetLine      =   256; // line when V state presets
	const int kVSyncLines       =     4; // lines per VSync duration

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
bool Video::VideoGetVblBarEx(const DWORD dwCyclesThisFrame)
{
	if (g_bFullSpeed)
	{
		// Ensure that NTSC video-scanner gets updated during full-speed, so video screen can be redrawn during Apple II VBL
		NTSC_VideoClockResync(dwCyclesThisFrame);
	}

	return g_nVideoClockVert < kVDisplayableScanLines;
}

// Called when *inside* CpuExecute()
bool Video::VideoGetVblBar(const DWORD uExecutedCycles)
{
	if (g_bFullSpeed)
	{
		// Ensure that NTSC video-scanner gets updated during full-speed, so video-dependent Apple II code doesn't hang
		NTSC_VideoClockResync(CpuGetCyclesThisVideoFrame(uExecutedCycles));
	}

	return g_nVideoClockVert < kVDisplayableScanLines;
}

//===========================================================================

void Video::Video_ResetScreenshotCounter(const std::string& pImageName)
{
	g_nLastScreenShot = 0;
	g_pLastDiskImageName = pImageName;
}

void Video::Util_MakeScreenShotFileName(TCHAR *pFinalFileName_, DWORD chars)
{
	const std::string sPrefixScreenShotFileName = "AppleWin_ScreenShot";
	// TODO: g_sScreenshotDir
	const std::string pPrefixFileName = !g_pLastDiskImageName.empty() ? g_pLastDiskImageName : sPrefixScreenShotFileName;
	StringCbPrintf( pFinalFileName_, chars, TEXT("%s_%09d.bmp"), pPrefixFileName.c_str(), g_nLastScreenShot );
}

// Returns TRUE if file exists, else FALSE
bool Video::Util_TestScreenShotFileName(const TCHAR *pFileName)
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

void Video::Video_TakeScreenShot(const VideoScreenShot_e ScreenShotType)
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

		Util_MakeScreenShotFileName(sScreenShotFileName, MAX_PATH);
		bExists = Util_TestScreenShotFileName(sScreenShotFileName);
		if( !bExists )
		{
			break;
		}
		g_nLastScreenShot++;
	}

	Video_SaveScreenShot(ScreenShotType, sScreenShotFileName);
	g_nLastScreenShot++;
}

void Video::Video_SetBitmapHeader(WinBmpHeader_t *pBmp, int nWidth, int nHeight, int nBitsPerPixel)
{
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

void Video::Video_MakeScreenShot(FILE *pFile, const VideoScreenShot_e ScreenShotType)
{
	WinBmpHeader_t *pBmp = &g_tBmpHeader;

	Video_SetBitmapHeader(
		pBmp,
		ScreenShotType == SCREENSHOT_280x192 ? GetFrameBufferBorderlessWidth()/2 : GetFrameBufferBorderlessWidth(),
		ScreenShotType == SCREENSHOT_280x192 ? GetFrameBufferBorderlessHeight()/2 : GetFrameBufferBorderlessHeight(),
		32
	);

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
}

//===========================================================================

void Video::Video_SaveScreenShot(const VideoScreenShot_e ScreenShotType, const TCHAR *pScreenShotFileName)
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

bool Video::ReadVideoRomFile(const char* pRomFile)
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

UINT Video::GetVideoRom(const BYTE*& pVideoRom)
{
	pVideoRom = &g_videoRom[0];
	return g_videoRomSize;
}

bool Video::GetVideoRomRockerSwitch(void)
{
	return g_videoRomRockerSwitch;
}

void Video::SetVideoRomRockerSwitch(bool state)
{
	g_videoRomRockerSwitch = state;
}

bool Video::IsVideoRom4K(void)
{
	return g_videoRomSize <= kVideoRomSize4K;
}

//===========================================================================

void Video::Config_Load_Video()
{
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

void Video::Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE)      ,g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_STYLE)     ,g_eVideoStyle);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR),g_nMonochromeRGB);
	REGSAVE(TEXT(REGVALUE_VIDEO_REFRESH_RATE), GetVideoRefreshRate());
}

//===========================================================================

uint32_t Video::GetVideoMode(void)
{
	return g_uVideoMode;
}

void Video::SetVideoMode(uint32_t videoMode)
{
	g_uVideoMode = videoMode;
}

VideoType_e Video::GetVideoType(void)
{
	return (VideoType_e) g_eVideoType;
}

// TODO: Can only do this at start-up (mid-emulation requires a more heavy-weight video reinit)
void Video::SetVideoType(VideoType_e newVideoType)
{
	g_eVideoType = newVideoType;
}

VideoStyle_e Video::GetVideoStyle(void)
{
	return g_eVideoStyle;
}

void Video::IncVideoType(void)
{
	g_eVideoType++;
	if (g_eVideoType >= NUM_VIDEO_MODES)
		g_eVideoType = 0;
}

void Video::DecVideoType(void)
{
	if (g_eVideoType <= 0)
		g_eVideoType = NUM_VIDEO_MODES;
	g_eVideoType--;
}

void Video::SetVideoStyle(VideoStyle_e newVideoStyle)
{
	g_eVideoStyle = newVideoStyle;
}

bool Video::IsVideoStyle(VideoStyle_e mask)
{
	return (g_eVideoStyle & mask) != 0;
}

//===========================================================================

VideoRefreshRate_e Video::GetVideoRefreshRate(void)
{
	return (g_bVideoScannerNTSC == false) ? VR_50HZ : VR_60HZ;
}

void Video::SetVideoRefreshRate(VideoRefreshRate_e rate)
{
	if (rate != VR_50HZ)
		rate = VR_60HZ;

	g_bVideoScannerNTSC = (rate == VR_60HZ);
	NTSC_SetRefreshRate(rate);
}

//===========================================================================

const char* Video::VideoGetAppWindowTitle(void)
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

void Video::VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit /*=false*/)
{
	if (bInit)
	{
		// Just entered full-speed mode
		dwFullSpeedStartTime = GetTickCount();
		return;
	}

	DWORD dwFullSpeedDuration = GetTickCount() - dwFullSpeedStartTime;
	if (dwFullSpeedDuration <= 16)	// Only update after every realtime ~17ms of *continuous* full-speed
		return;

	dwFullSpeedStartTime += dwFullSpeedDuration;

	VideoRedrawScreenAfterFullSpeed(dwCyclesThisFrame);
}

void Video::VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame)
{
	NTSC_VideoClockResync(dwCyclesThisFrame);
	GetFrame().VideoRedrawScreen();	// Better (no flicker) than using: NTSC_VideoReinitialize() or VideoReinitialize()
}

void Video::Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename)
{
	_ASSERT(pScreenshotFilename);
	if (!pScreenshotFilename)
		return;

	GetFrame().VideoRedrawScreen();
	Video_SaveScreenShot(SCREENSHOT_560x384, pScreenshotFilename);
}

void Video::VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode, bool bRedrawWholeScreen)
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

	VideoPresentScreen();
}
