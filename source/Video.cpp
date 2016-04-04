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
#include "Debugger\Debugger_Color.h"	// For NUM_DEBUG_COLORS
#include "YamlHelper.h"

#define HALF_PIXEL_SOLID 1
#define HALF_PIXEL_BLEED 0

#define HALF_DIM_SUPPORT 0

/*
   Reference: Technote TN-IIGS-063 "Master Color Values"
   Note:The IIGS colors do NOT map correctly to _accurate_ //e colors.

          Color       LO HI  DHR Master Color R,G,B                         HGR
          Name        #  #   #      Value                                   Bytes
          -----------------------------------------------------------------------
          Black       0  0,4 0      $0000    (0,0,0) -> (00,00,00) Windows 
(Magenta) Deep Red    1      1      $0D03    (D,0,3) -> (D0,00,30) Custom
          Dark Blue   2      8      $0009    (0,0,9) -> (00,00,80) Windows
 (Violet) Purple      3  2   9      $0D2D    (D,2,D) -> (FF,00,FF) Windows  55 2A
          Dark Green  4      4      $0072    (0,7,2) -> (00,80,00) Windows
 (Gray 1) Dark Gray   5      5      $0555    (5,5,5) -> (80,80,80) Windows
   (Blue) Medium Blue 6  6   C      $022F    (2,2,F) -> (00,00,FF) Windows  D5 AA
   (Cyan) Light Blue  7      D      $06AF    (6,A,F) -> (60,A0,FF) Custom
          Brown       8      2      $0850    (8,5,0) -> (80,50,00) Custom
          Orange      9  5   3      $0F60    (F,6,0) -> (FF,80,00) Custom   AA D5 (modified to match better with the other Hi-Res Colors)
 (Gray 2) Light Gray  A      A      $0AAA    (A,A,A) -> (C0,C0,C0) Windows
          Pink        B      B      $0F98    (F,9,8) -> (FF,90,80) Custom
  (Green) Light Green C  1   6      $01D0    (1,D,0) -> (00,FF,00) Windows  2A 55
          Yellow      D      7      $0FF0    (F,F,0) -> (FF,FF,00) Windows
   (Aqua) Aquamarine  E      E      $04F9    (4,F,9) -> (40,FF,90) Custom
          White       F  3,7 F      $0FFF    (F,F,F) -> (FF,FF,FF) Windows

   Legend:
       LO: Lo-Res
       HI: Hi-Res
      DHR: Double Hi-Res
*/

#define FLASH_80_COL 1	// Bug #7238

#define HALF_SHIFT_DITHER 0

enum Color_Palette_Index_e
{
	// The first 10 are the DEFAULT Windows colors (as it reserves 20 colors)
	  BLACK        = 0x00 // 0x00,0x00,0x00 
	, DARK_RED     = 0x01 // 0x80,0x00,0x00 
	, DARK_GREEN   = 0x02 // 0x00,0x80,0x00 (Half Green)
	, DARK_YELLOW  = 0x03 // 0x80,0x80,0x00
	, DARK_BLUE    = 0x04 // 0x00,0x00,0x80 (Half Blue)
	, DARK_MAGENTA = 0x05 // 0x80,0x00,0x80
	, DARK_CYAN    = 0x06 // 0x00,0x80,0x80
	, LIGHT_GRAY   = 0x07 // 0xC0,0xC0,0xC0
	, MONEY_GREEN  = 0x08 // 0xC0,0xDC,0xC0 // not used
	, SKY_BLUE     = 0x09 // 0xA6,0xCA,0xF0 // not used

// Really need to have Quarter Green and Quarter Blue for Hi-Res
// OUR CUSTOM COLORS -- the extra colors HGR mode can display
//	, DEEP_BLUE // Breaks TV Emulation Reference Test !?!?   // Breaks the dam palette -- black monochrome TEXT output bug *sigh*
	, DEEP_RED         
	, LIGHT_BLUE       
	, BROWN            
	, ORANGE           
	, PINK             
	, AQUA             

// CUSTOM HGR COLORS (don't change order) - For tv emulation HGR Video Mode
	, HGR_BLACK        
	, HGR_WHITE        
	, HGR_BLUE         // HCOLOR=6 BLUE   , 3000: 81 00 D5 AA
	, HGR_ORANGE       // HCOLOR=5 ORANGE , 2C00: 82 00 AA D5
	, HGR_GREEN        // HCOLOR=1 GREEN  , 2400: 02 00 2A 55
	, HGR_VIOLET       // HCOLOR=2 VIOLET , 2800: 01 00 55 2A
	, HGR_GREY1        
	, HGR_GREY2        
	, HGR_YELLOW       
	, HGR_AQUA         
	, HGR_PURPLE       
	, HGR_PINK         

// MONOCHROME
	, MONOCHROME_CUSTOM     // 100% luminance
	, MONOCHROME_CUSTOM_50  //  50% luminance
	, MONOCHROME_AMBER
	, MONOCHROME_GREEN
	, DEBUG_COLORS_START
	, DEBUG_COLORS_END = DEBUG_COLORS_START + NUM_DEBUG_COLORS

	, NUM_COLOR_PALETTE

	// The last 10 are the DEFAULT Windows colors (as it reserves 20 colors)
	, CREAM       = 0xF6
	, MEDIUM_GRAY = 0xF7
	, DARK_GRAY   = 0xF8
	, RED         = 0xF9
	, GREEN       = 0xFA
	, YELLOW      = 0xFB
	, BLUE        = 0xFC
	, MAGENTA     = 0xFD
	, CYAN        = 0xFE
	, WHITE       = 0xFF
};

// __ Map HGR color index to Palette index
	enum ColorMapping
	{
		  CM_Violet
		, CM_Blue
		, CM_Green
		, CM_Orange
		, CM_Black // Used
		, CM_White // Used
		, NUM_COLOR_MAPPING
	};

const BYTE HiresToPalIndex[ NUM_COLOR_MAPPING ] =
	{
		  HGR_VIOLET
		, HGR_BLUE
		, HGR_GREEN
		, HGR_ORANGE
		, HGR_BLACK
		, HGR_WHITE
	};

const BYTE LoresResColors[16] = {
		BLACK,     DEEP_RED, DARK_BLUE, MAGENTA,
		DARK_GREEN,DARK_GRAY,BLUE,      LIGHT_BLUE,
		BROWN,     ORANGE,   LIGHT_GRAY,PINK,
		GREEN,     YELLOW,   AQUA,      HGR_WHITE
	};


const BYTE DoubleHiresPalIndex[16] = {
		BLACK,   DARK_BLUE, DARK_GREEN,BLUE,
		BROWN,   LIGHT_GRAY, GREEN,     AQUA,
		DEEP_RED,MAGENTA,   DARK_GRAY, LIGHT_BLUE,
		ORANGE,  PINK,      YELLOW,    HGR_WHITE
	};

	const int SRCOFFS_40COL   = 0;                       //    0
	const int SRCOFFS_IIPLUS  = (SRCOFFS_40COL  +  256); //  256
	const int SRCOFFS_80COL   = (SRCOFFS_IIPLUS +  256); //  512
	const int SRCOFFS_LORES   = (SRCOFFS_80COL  +  128); //  640
	const int SRCOFFS_HIRES   = (SRCOFFS_LORES  +   16); //  656
	const int SRCOFFS_DHIRES  = (SRCOFFS_HIRES  +  512); // 1168
	const int SRCOFFS_TOTAL   = (SRCOFFS_DHIRES + 2560); // 3278

	#define  SW_80COL         (g_uVideoMode & VF_80COL)
	#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
	#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
	#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
	#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
	#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
	#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

#define  SETSOURCEPIXEL(x,y,c)  g_aSourceStartofLine[(y)][(x)] = (c)

#define  SETFRAMECOLOR(i,r,g,b)  g_pFramebufferinfo->bmiColors[i].rgbRed      = r; \
                                 g_pFramebufferinfo->bmiColors[i].rgbGreen    = g; \
                                 g_pFramebufferinfo->bmiColors[i].rgbBlue     = b; \
								 g_pFramebufferinfo->bmiColors[i].rgbReserved = PC_NOCOLLAPSE;

#define  HGR_MATRIX_YOFFSET 2	// For tv emulation HGR Video Mode

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

static COLORREF      customcolors[256];	// MONOCHROME is last custom color

static HBITMAP       g_hDeviceBitmap;
static HDC           g_hDeviceDC;
static LPBITMAPINFO  g_pFramebufferinfo = NULL;

static LPBYTE        g_aFrameBufferOffset[FRAMEBUFFER_H]; // array of pointers to start of each scanline
static LPBYTE        g_pHiresBank1;
static LPBYTE        g_pHiresBank0;
       HBITMAP       g_hLogoBitmap;
static HPALETTE      g_hPalette;

static HBITMAP       g_hSourceBitmap;
static LPBYTE        g_pSourcePixels;
static LPBITMAPINFO  g_pSourceHeader;
const int MAX_SOURCE_Y = 512;
static LPBYTE        g_aSourceStartofLine[ MAX_SOURCE_Y ];
static LPBYTE        g_pTextBank1; // Aux
static LPBYTE        g_pTextBank0; // Main

static /*bool*/ UINT g_VideoForceFullRedraw = 1;

static LPBYTE    framebufferaddr  = (LPBYTE)0;
static LONG      g_nFrameBufferPitch = 0;
COLORREF         g_nMonochromeRGB    = RGB(0xC0,0xC0,0xC0);
static BOOL      rebuiltsource       = 0;
static LPBYTE    vidlastmem          = NULL;

	uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

	DWORD     g_eVideoType     = VT_COLOR_TVEMU;
	DWORD     g_uHalfScanLines = 1; // drop 50% scan lines for a more authentic look

static bool bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

//-------------------------------------

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	TCHAR g_aVideoChoices[] =
		TEXT("Monochrome (Custom Luminance)\0")
		TEXT("Color (Standard)\0")
		TEXT("Color (Text Optimized)\0")
		TEXT("Color (TV emulation)\0")
		TEXT("Monochrome (Amber)\0")
		TEXT("Monochrome (Green)\0")
		TEXT("Monochrome (White)\0")
		;

	// AppleWin 1.19.4 VT_COLOR_AUTHENTIC -> VT_COLOR_HALFPIXEL -> VT_COLOR_STANDARD "Color Half-Pixel Authentic
	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	// The window title will be set to this.
	char *g_apVideoModeDesc[ NUM_VIDEO_MODES ] =
	{
		  "Monochrome (Custom)"
		, "Standard"        
		, "Text Optimized"
		, "TV"
		, "Amber"
		, "Green"
		, "White"
	};

// Prototypes (Private) _____________________________________________

// Monochrome Half-Pixel Support
	void V_CreateLookup_MonoHiResHalfPixel_Real ();

	bool g_bDisplayPrintScreenFileName = false;
	bool g_bShowPrintScreenWarningDialog = true;
	void Util_MakeScreenShotFileName( char *pFinalFileName_ );
	bool Util_TestScreenShotFileName( const char *pFileName );
	// true  = 280x192
	// false = 560x384
	void Video_SaveScreenShot( const char *pScreenShotFileName );
	void Video_MakeScreenShot( FILE *pFile );

	int GetMonochromeIndex();

	void V_CreateIdentityPalette ();
	void  videoCreateDIBSection();

//===========================================================================
void CreateFrameOffsetTable (LPBYTE addr, LONG pitch)
{
	if ((framebufferaddr  == addr) && (g_nFrameBufferPitch == pitch))
		return;

	framebufferaddr  = addr;
	g_nFrameBufferPitch = pitch;

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE FRAME BUFFER
	for (int y = 0; y < FRAMEBUFFER_H; y++)
		g_aFrameBufferOffset[y] = framebufferaddr + g_nFrameBufferPitch*((FRAMEBUFFER_H-1)-y);
}

//===========================================================================
void VideoInitialize ()
{
	// RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
	VideoResetState();

	// CREATE A BUFFER FOR AN IMAGE OF THE LAST DRAWN MEMORY
	vidlastmem = (LPBYTE)VirtualAlloc(NULL,0x10000,MEM_COMMIT,PAGE_READWRITE);
	ZeroMemory(vidlastmem,0x10000);

	// LOAD THE LOGO
//	g_hLogoBitmap = (HBITMAP)LoadImage(g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN), IMAGE_BITMAP, 560, 384, LR_CREATEDIBSECTION);
	g_hLogoBitmap = LoadBitmap( g_hInstance, MAKEINTRESOURCE(IDB_APPLEWIN) );

	// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
	g_pFramebufferinfo = (LPBITMAPINFO)VirtualAlloc(
		NULL,
		sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD),
		MEM_COMMIT,
		PAGE_READWRITE);

	ZeroMemory(g_pFramebufferinfo,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
	g_pFramebufferinfo->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	g_pFramebufferinfo->bmiHeader.biWidth       = FRAMEBUFFER_W;
	g_pFramebufferinfo->bmiHeader.biHeight      = FRAMEBUFFER_H;
	g_pFramebufferinfo->bmiHeader.biPlanes      = 1;
	g_pFramebufferinfo->bmiHeader.biBitCount    = 32;
	g_pFramebufferinfo->bmiHeader.biCompression = BI_RGB;
	g_pFramebufferinfo->bmiHeader.biClrUsed     = 0;

	videoCreateDIBSection();
}

//===========================================================================
void V_CreateIdentityPalette ()
{
	SETFRAMECOLOR(BLACK,       0x00,0x00,0x00); // 0
	SETFRAMECOLOR(DARK_RED,    0x80,0x00,0x00); // 1 // not used
	SETFRAMECOLOR(DARK_GREEN,  0x00,0x80,0x00); // 2 // not used
	SETFRAMECOLOR(DARK_YELLOW, 0x80,0x80,0x00); // 3
	SETFRAMECOLOR(DARK_BLUE,   0x00,0x00,0x80); // 4 // not used
	SETFRAMECOLOR(DARK_MAGENTA,0x80,0x00,0x80); // 5
	SETFRAMECOLOR(DARK_CYAN,   0x00,0x80,0x80); // 6
	SETFRAMECOLOR(LIGHT_GRAY,  0xC0,0xC0,0xC0); // 7 // GR: COLOR=10
	SETFRAMECOLOR(MONEY_GREEN, 0xC0,0xDC,0xC0); // 8 // not used
	SETFRAMECOLOR(SKY_BLUE,    0xA6,0xCA,0xF0); // 9 // not used

	// SET FRAME BUFFER TABLE ENTRIES TO CUSTOM COLORS
//	SETFRAMECOLOR(DARK_RED,    0x9D,0x09,0x66); // 1 // Linards Tweaked 0x80,0x00,0x00 -> 0x9D,0x09,0x66
//	SETFRAMECOLOR(DARK_GREEN,  0x00,0x76,0x1A); // 2 // Linards Tweaked 0x00,0x80,0x00 -> 0x00,0x76,0x1A
//	SETFRAMECOLOR(DARK_BLUE,   0x2A,0x2A,0xE5); // 4 // Linards Tweaked 0x00,0x00,0x80 -> 0x2A,0x2A,0xE5

	SETFRAMECOLOR(DEEP_RED,  0x9D,0x09,0x66); // 0xD0,0x00,0x30 -> Linards Tweaked 0x9D,0x09,0x66
	SETFRAMECOLOR(LIGHT_BLUE,0xAA,0xAA,0xFF); // 0x60,0xA0,0xFF -> Linards Tweaked 0xAA,0xAA,0xFF
	SETFRAMECOLOR(BROWN,     0x55,0x55,0x00); // 0x80,0x50,0x00 -> Linards Tweaked 0x55,0x55,0x00
	SETFRAMECOLOR(ORANGE,    0xF2,0x5E,0x00); // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
	SETFRAMECOLOR(PINK,      0xFF,0x89,0xE5); // 0xFF,0x90,0x80 -> Linards Tweaked 0xFF,0x89,0xE5
	SETFRAMECOLOR(AQUA,      0x62,0xF6,0x99); // 0x40,0xFF,0x90 -> Linards Tweaked 0x62,0xF6,0x99

	SETFRAMECOLOR(HGR_BLACK,  0x00,0x00,0x00); // For TV emulation HGR Video Mode
	SETFRAMECOLOR(HGR_WHITE,  0xFF,0xFF,0xFE); // BUG: PALETTE COLLAPSE!  NOT white!? Win32 collapses the palette if you have duplicate colors!
// 20 207 253 = 0x14 0xCF 0xFD
	SETFRAMECOLOR(HGR_BLUE,     24, 115, 229); // HCOLOR=6 BLUE    3000: 81 00 D5 AA // 0x00,0x80,0xFF -> Linards Tweaked 0x0D,0xA1,0xFF
	SETFRAMECOLOR(HGR_ORANGE,  247,  64,  30); // HCOLOR=5 ORANGE  2C00: 82 00 AA D5 // 0xF0,0x50,0x00 -> Linards Tweaked 0xF2,0x5E,0x00 
	SETFRAMECOLOR(HGR_GREEN,    27, 211,  79); // HCOLOR=1 GREEN   2400: 02 00 2A 55 // 0x20,0xC0,0x00 -> Linards Tweaked 0x38,0xCB,0x00
	SETFRAMECOLOR(HGR_VIOLET,  227,  20, 255); // HCOLOR=2 VIOLET  2800: 01 00 55 2A // 0xA0,0x00,0xFF -> Linards Tweaked 0xC7,0x34,0xFF

	SETFRAMECOLOR(HGR_GREY1,  0x80,0x80,0x80);
	SETFRAMECOLOR(HGR_GREY2,  0x80,0x80,0x80);
	SETFRAMECOLOR(HGR_YELLOW, 0x9E,0x9E,0x00); // 0xD0,0xB0,0x10 -> 0x9E,0x9E,0x00
	SETFRAMECOLOR(HGR_AQUA,   0x00,0xCD,0x4A); // 0x20,0xB0,0xB0 -> 0x00,0xCD,0x4A
	SETFRAMECOLOR(HGR_PURPLE, 0x61,0x61,0xFF); // 0x60,0x50,0xE0 -> 0x61,0x61,0xFF
	SETFRAMECOLOR(HGR_PINK,   0xFF,0x32,0xB5); // 0xD0,0x40,0xA0 -> 0xFF,0x32,0xB5

	SETFRAMECOLOR( MONOCHROME_CUSTOM
		, GetRValue(g_nMonochromeRGB)
		, GetGValue(g_nMonochromeRGB)
		, GetBValue(g_nMonochromeRGB)
	);

	SETFRAMECOLOR( MONOCHROME_CUSTOM_50
		, ((GetRValue(g_nMonochromeRGB)/2) & 0xFF)
		, ((GetGValue(g_nMonochromeRGB)/2) & 0xFF)
		, ((GetBValue(g_nMonochromeRGB)/2) & 0xFF)
	);

	SETFRAMECOLOR( MONOCHROME_AMBER   , 0xFF,0x80,0x01); // Used for Monochrome Hi-Res graphics not text!
	SETFRAMECOLOR( MONOCHROME_GREEN   , 0x00,0xC0,0x01); // Used for Monochrome Hi-Res graphics not text!
	// BUG PALETTE COLLAPSE: WTF?? Soon as we set 0xFF,0xFF,0xFF we lose text colors?!?!
	// Windows is collapsing the palette!!!
	//SETFRAMECOLOR( MONOCHROME_WHITE   , 0xFE,0xFE,0xFE); // Used for Monochrome Hi-Res graphics not text!

	SETFRAMECOLOR(CREAM,       0xFF,0xFB,0xF0); // F6
	SETFRAMECOLOR(MEDIUM_GRAY, 0xA0,0xA0,0xA4); // F7
	SETFRAMECOLOR(DARK_GRAY,   0x80,0x80,0x80); // F8
	SETFRAMECOLOR(RED,         0xFF,0x00,0x00); // F9
	SETFRAMECOLOR(GREEN,       0x38,0xCB,0x00); // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
	SETFRAMECOLOR(YELLOW,      0xD5,0xD5,0x1A); // FB Linards Tweaked 0xFF,0xFF,0x00 -> 0xD5,0xD5,0x1A
	SETFRAMECOLOR(BLUE,        0x0D,0xA1,0xFF); // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
	SETFRAMECOLOR(MAGENTA,     0xC7,0x34,0xFF); // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF
	SETFRAMECOLOR(CYAN,        0x00,0xFF,0xFF); // FE
	SETFRAMECOLOR(WHITE,       0xFF,0xFF,0xFF); // FF
}

#if 0
/*
aPixel[i]
 A 9|8 7 6 5 4 3 2|1 0
 Z W|b b b b b b b|X Y
----+-------------+----
prev|  existing   |next
bits| hi-res byte |bits

Legend:
 XYZW = iColumn in binary
 b = Bytes in binary
*/
			// aPixel[] = 48bbbbbbbb12, where b = iByte in binary, # is bit-n of column
			aPixels[ 0] = iColumn & 4; // previous byte, 2nd last pixel
			aPixels[ 1] = iColumn & 8; // previous byte, last pixel
			aPixels[ 9] = iColumn & 1; // next byte, first pixel
			aPixels[10] = iColumn & 2; // next byte, second pixel

			// Convert raw pixel Byte value to binary and stuff into bit array of pixels on off
			int nBitMask = 1;
			int iPixel;
			for (iPixel  = 2; iPixel < 9; iPixel++)
			{
				aPixels[iPixel] = ((iByte & nBitMask) != 0);
				nBitMask <<= 1;
			}

			int hibit = (iByte >> 7) & 1; // ((iByte & 0x80) != 0);
			int x     = 0;
			int y     = iByte << 1;

/*
Color Reference Tests:

2000:D5 AA D5 AA D5 AA //  blue blue  blue
2400:AA D5 2A 55 55 2A //+ red  green violet
//                     //= grey aqua  violet

2C00:AA D5 AA D5 2A 55 //  red    red     green
3000:2A 55 55 2A 55 2A //+ green  violet  violet
//                     //= yellow pink    grey

Test cases
==========
 81 blue
   2000:D5 AA D5 AA
 82 orange
   2800:AA D5 AA D5
 FF white bleed "thru"
   3000:7F 80 7F 80
   3800:FF 80 FF 80
   2028:80 7F 80 7F
   2828:80 FF 80 FF
 Edge Case for Half Luminance !
   2000:C4 00  // Green  HalfLumBlue
   2400:C4 80  // Green  Green
 Edge Case for Color Bleed !
   2000:40 00
   2400:40 80

// Test Patterns 
// 81 blue
//   2000:D5 AA D5 AA -> 2001:AA D5  should not have black gap, should be blue
// 82 orange
//   2800:AA D5 AA D5
// Game: Elite -- Loading Logo 
//   2444:BB F7 -> 2000:BB F7    // Should not have orange in-between gap -- Elite "Firebird" Logo
//              -> 2400:00 BB F7 // Should not have blue in-between gap )
//   21D0:C0 00    -> HalfLumBlue
//   25D0:C0 D0 88 -> Blue black orange black orange
//   29D0:C0 90 88 -> Blue black orange
// Game: Ultima 4 -- Ultima 4 Logo - bottom half of screen has a "mini-game" / demo -- far right has tree and blue border
//   2176:2A AB green black_gap white blue_border // Should have black gap between green and white
// Game: Gumball
//   218E:AA 97    => 2000: A9 87          orange_white            // Should have no gap between orange and white
//   229A:AB A9 87 -> 2000: 00 A9 87 white orange black blue_white // Should have no gap between blue and white
//   2001:BB F7                            white blue white  (Gumball Intermission)
// Torture Half-Pixel HGR Tests:  This is a real bitch to solve -- we really need to check:
//     if (hibit_prev_byte && !aPixels[iPixel-3] && aPixels[iPixel-2] && !aPixels[iPixel] && hibit_this_byte) then set first half-pixel of this byte to either blue or orange
//   2000:A9 87 halfblack blue black black orange black orange black
//   2400:BB F7 halfblack white white black white white white halfblack
//  or
//   2000:A0 83 orange should "bleed" thru
//   2400:B0 83 should have black gap
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , DARK_BLUE ); // Gumball: 229A: AB A9 87
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , BROWN ); // half luminance red Elite: 2444: BB F7
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, BROWN ); // half luminance red Gumball: 218E: AA 97
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , HGR_BLUE ); // 2000:D5 AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , HGR_ORANGE ); // 2000: AA D5
							// Test Pattern: Ultima 4 Logo - Castle
							// 3AC8: 36 5B 6D 36
						Address Binary   -> Displayed
						2000:01 0---0001 -> 1 0 0 0  column 1
						2400:81 1---0001 ->  1 0 0 0 half-pixel shift right
						2800:02 1---0010 -> 0 1 0 0  column 2

						2000:02 column 2
						2400:82 half-pixel shift right
						2800:04 column 3

						2000:03 0---0011 -> 1 1 0 0  column 1 & 2
						2400:83 1---0011 ->  1 1 0 0 half-pixel shift right
						2800:06 1---0110 -> 0 1 1 0  column 2 & 3

						@reference: see Beagle Bro's Disk: "Silicon Salad", File: DOUBLE HI-RES
						Fortunately double-hires is supported via pixel doubling, so we can do half-pixel shifts ;-)
					// Games
						Archon Logo
						Gumball (at Machine)
					// Applesoft
						HGR:HCOLOR=5:HPLOT 0,0 TO 279,0

					// Blue
					// Orange
						CALL-151
						C050 C052 C057
						2000:D0 80 00
						2800:80 D0 00
*/
#endif

// google: CreateDIBPatternBrushPt
// http://209.85.141.104/search?q=cache:mB3htrQGW8kJ:bookfire.net/wince/wince-programming-ms-press2/source/prowice/ch02e.htm

struct BRUSHBMP
{
    BITMAPINFOHEADER bmi;
    COLORREF dwPal[2];
    BYTE bBits[64];
};

//===========================================================================

static void CreateLookup_TextCommon(HDC hDstDC, DWORD rop)
{
	HBITMAP hCharBitmap[4];
	HDC     hSrcDC = CreateCompatibleDC(hDstDC);

	hCharBitmap[0] = LoadBitmap(g_hInstance,TEXT("CHARSET40"));
	hCharBitmap[1] = LoadBitmap(g_hInstance,TEXT("CHARSET82"));
	hCharBitmap[2] = LoadBitmap(g_hInstance,TEXT("CHARSET8C")); // FIXME: Pravets 8M probably has the same charset as Pravets 8C
	hCharBitmap[3] = LoadBitmap(g_hInstance,TEXT("CHARSET8C"));
	SelectObject(hSrcDC, hCharBitmap[g_nCharsetType]);

	// TODO: Update with APPLE_FONT_Y_ values
	BitBlt(    hDstDC, SRCOFFS_40COL ,0,256,512,hSrcDC,0,  0,rop);
	BitBlt(    hDstDC, SRCOFFS_IIPLUS,0,256,256,hSrcDC,0,512,rop);			// Chars for Apple ][
	StretchBlt(hDstDC, SRCOFFS_80COL ,0,128,512,hSrcDC,0,  0,256,512,rop);	// Chars for 80 col mode

	DeleteDC(hSrcDC);
	for (UINT i=0; i<4; i++)
		DeleteObject(hCharBitmap[i]); 
}

//===========================================================================

static inline int GetOriginal2EOffset(BYTE ch)
{
	// No mousetext for original IIe
	return !IsOriginal2E() || !g_nAltCharSetOffset || (ch<0x40) || (ch>0x5F) ? 0 : -g_nAltCharSetOffset;
}

//===========================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL VideoApparentlyDirty ()
{
	if (SW_MIXED || g_VideoForceFullRedraw)
		return 1;

	DWORD address = (SW_HIRES && !SW_TEXT)
		? (0x20 << (SW_PAGE2 ? 1 : 0))
		: (0x04 << (SW_PAGE2 ? 1 : 0));
	DWORD length  = (SW_HIRES && !SW_TEXT) ? 0x20 : 0x4;
	while (length--)
		if (*(memdirty+(address++)) & 2)
			return 1;

	//

	// Scan visible text page for any flashing chars
	if((SW_TEXT || SW_MIXED) && (g_nAltCharSetOffset == 0))
	{
		BYTE* pTextBank0  = MemGetMainPtr(0x400 << (SW_PAGE2 ? 1 : 0));
		BYTE* pTextBank1  = MemGetAuxPtr (0x400 << (SW_PAGE2 ? 1 : 0));
		const bool b80Col = SW_80COL;

		// Scan 8 long-lines of 120 chars (at 128 char offsets):
		// . Skip 8-char holes in TEXT
		for(UINT y=0; y<8; y++)
		{
			for(UINT x=0; x<40*3; x++)
			{
				BYTE ch = pTextBank0[y*128+x];
				if((ch >= 0x40) && (ch <= 0x7F))
					return 1;

				if (b80Col)
				{
					ch = pTextBank1[y*128+x];
					if((ch >= 0x40) && (ch <= 0x7F))
						return 1;
				}
			}
		}
	}

	return 0;
}

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
    VideoRefreshScreen(0);
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
    VideoRefreshScreen(0);
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
    CpuExecute(100000);
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
        CpuExecute(loop);
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
        DWORD executedcycles = CpuExecute(103);
        cycles -= executedcycles;
        DiskUpdatePosition(executedcycles);
        JoyUpdateButtonLatch(executedcycles);
	  }
    }
    if (cycle & 1)
      FillMemory(mem+0x2000,0x2000,0xAA);
    else
      CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoRedrawScreen(); // VideoRefreshScreen();
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

/*
	// Drol expects = 80
	68DE A5 02    LDX #02
	68E0 AD 50 C0 LDA TXTCLR
	68E3 C9 80    CMP #80
	68E5 D0 F7    BNE $68DE

	6957 A5 02    LDX #02
	6959 AD 50 C0 LDA TXTCLR
	695C C9 80    CMP #80
	695E D0 F7    BNE $68DE

	69D3 A5 02    LDX #02
	69D5 AD 50 C0 LDA TXTCLR
	69D8 C9 80    CMP #80
	69DA D0 F7    BNE $68DE

	// Karateka expects < 80
	07DE AD 19 C0 LDA RDVBLBAR
	07E1 30 FB    BMI $7DE

	77A1 AD 19 C0 LDA RDVBLBAR
	77A4 30 FB    BMI $7DE

	// Gumball expects non-zero low-nibble on VBL
	BBB5 A5 60    LDA $60
	BBB7 4D 50 C0 EOR TXTCLR
	BBBA 85 60    STA $60
	BBBC 29 0F    AND #$0F
	BBBE F0 F5    BEQ $BBB5
	BBC0 C9 0F    CMP #$0F
	BBC2 F0 F1    BEQ $BBB5

	// Diversi-Dial (DD4.DSK or DIAL.DSK)
	F822          LDA RDVBLBAR
	F825          EOR #$3C
	              BMI $F82A
	              RTS
	F82A          LDA $F825+1
	              EOR #$80
	              STA $F825+1
	              BMI $F86A
				  ...
	F86A          RTS

*/

BYTE VideoCheckVbl ( ULONG uExecutedCycles )
{
	bool bVblBar = VideoGetVbl(uExecutedCycles);
	// NTSC: It is tempting to replace with
	//     bool bVblBar = NTSC_VideoIsVbl();
	// But this breaks "ANSI STORY" intro center fade

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
  VirtualFree(g_pSourceHeader     ,0,MEM_RELEASE);
  VirtualFree(vidlastmem     ,0,MEM_RELEASE);
  g_pFramebufferinfo = NULL;
  g_pSourceHeader      = NULL;
  vidlastmem      = NULL;

  // DESTROY FRAME BUFFER
  DeleteDC(g_hDeviceDC);
  DeleteObject(g_hDeviceBitmap);
  g_hDeviceDC     = (HDC)0;
  g_hDeviceBitmap = (HBITMAP)0;

  // DESTROY SOURCE IMAGE
  DeleteObject(g_hSourceBitmap);
  g_hSourceBitmap = (HBITMAP)0;

  // DESTROY LOGO
  if (g_hLogoBitmap) {
    DeleteObject(g_hLogoBitmap);
    g_hLogoBitmap = (HBITMAP)0;
  }

  // DESTROY PALETTE
  if (g_hPalette) {
    DeleteObject(g_hPalette);
    g_hPalette = (HPALETTE)0;
  }
}

//===========================================================================

void VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale)
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

int g_nLogoWidth = FRAMEBUFFER_W;
int g_nLogoX     = 0;
int g_nLogoY     = 0;

//===========================================================================
void VideoDisplayLogo () 
{
	int xoff = 0, yoff = 0;
	const int scale = GetViewportScale();

	HDC hFrameDC = FrameGetDC();

	// DRAW THE LOGO
	SelectObject(hFrameDC, GetStockObject(NULL_PEN));

	if (g_hLogoBitmap)
	{
		BITMAP bm;
		if (GetObject(g_hLogoBitmap, sizeof(bm), &bm))
		{
			g_nLogoWidth = bm.bmWidth;
			g_nLogoX     = (g_nViewportCX - scale*g_nLogoWidth)/2;
			g_nLogoY     = (g_nViewportCY - scale*bm.bmHeight )/2;

			// Draw Logo at top of screen so when the Apple display is refreshed it will automagically clear it
			if( g_bIsFullScreen )
			{
				g_nLogoX = 0;
				g_nLogoY = 0;
			}
			VideoDrawLogoBitmap( hFrameDC, g_nLogoX, g_nLogoY, bm.bmWidth, bm.bmHeight, scale );
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

// NTSC Alpha Version
	DeleteObject(font);
/*
	font = CreateFontA(
		-48,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
		VARIABLE_PITCH | 4 | FF_SWISS,
		sFontName)
	);
*/
	PLOGFONT pLogFont = (PLOGFONT) LocalAlloc(LPTR, sizeof(LOGFONT));
	int angle = (int)(7.5 * 10); // 3600 = 360 degrees
	pLogFont->lfHeight = -48;
	pLogFont->lfWeight = FW_NORMAL;
	pLogFont->lfEscapement  = angle;
	pLogFont->lfOrientation = angle;
	SetTextAlign(hFrameDC,TA_BASELINE);

	font = CreateFontIndirect( pLogFont );
	HGDIOBJ  hFontPrev = SelectObject(hFrameDC, font);

	SelectObject(hFrameDC,font);
//	sprintf( szVersion, "NTSC Alpha v14 HorzClock" );
//	sprintf( szVersion, "NTSC Alpha v15 Fraps" );
//	sprintf( szVersion, "NTSC Alpha v16 Palette" );
//	sprintf( szVersion, "NTSC Alpha v17 BMP Palette" );
	sprintf( szVersion, "NTSC Alpha v18" );

	xoff = -g_nViewportCX + g_nViewportCX/6;
	yoff = -g_nViewportCY/16;
	DRAWVERSION( 0, 0,RGB(0x00,0x00,0x00));
	DRAWVERSION( 1, 1,RGB(0x00,0x00,0x00));
	DRAWVERSION( 2, 2,RGB(0xFF,0x00,0xFF));

 	sprintf( szVersion, "Blurry 80-col Text" );
	xoff = -g_nViewportCX + g_nViewportCX/6;
	yoff = +g_nViewportCY/16;
	DRAWVERSION( 0, 0,RGB(0x00,0x00,0x00));
	DRAWVERSION( 1, 1,RGB(0x00,0x00,0x00));
	DRAWVERSION( 2, 2,RGB(0xFF,0x00,0xFF));

	LocalFree((LOCALHANDLE)pLogFont);
	SelectObject(hFrameDC,hFontPrev);
// NTSC END

#undef  DRAWVERSION

	FrameReleaseVideoDC();

	DeleteObject(font);
}

//===========================================================================

void VideoRedrawScreen (UINT uDelayRefresh /* =0 */)
{
	g_VideoForceFullRedraw = 1;

	VideoRefreshScreen( g_uVideoMode, uDelayRefresh );
}

//===========================================================================
int _Video_SetupBanks( bool bBank2 )
{
	g_pHiresBank1 = MemGetAuxPtr (0x2000 << (int)bBank2);
	g_pHiresBank0 = MemGetMainPtr(0x2000 << (int)bBank2);
	g_pTextBank1  = MemGetAuxPtr (0x400  << (int)bBank2);
	g_pTextBank0  = MemGetMainPtr(0x400  << (int)bBank2);

	return bBank2 ? VF_PAGE2 : 0;
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

void VideoRefreshScreen ( int bVideoModeFlags, UINT uDelayRefresh /* =0 */ )
{
	static UINT uDelayRefreshCount = 0;
	if (uDelayRefresh) uDelayRefreshCount = uDelayRefresh;

#if defined(_DEBUG) && defined(DEBUG_REFRESH_TIMINGS)
	DebugRefresh(0);
#endif

	if( bVideoModeFlags )
	{
		NTSC_SetVideoMode( bVideoModeFlags );
		NTSC_VideoUpdateCycles( VIDEO_SCANNER_6502_CYCLES );
	}

// NTSC_BEGIN
	LPBYTE pDstFrameBufferBits = 0;
	LONG   pitch = 0;
	HDC    hFrameDC = FrameGetVideoDC(&pDstFrameBufferBits,&pitch);

#if 1 // Keep Aspect Ratio
	// Need to clear full screen logo to black
	#define W g_nViewportCX
	#define H g_nViewportCY
#else // Stretch
	// Stretch - doesn't preserve 1:1 aspect ratio
	#define W g_bIsFullScreen ? g_nDDFullScreenW : g_nViewportCX
	#define H g_bIsFullScreen ? g_nDDFullScreenH : g_nViewportCY
#endif

	if (hFrameDC)
	{
		if (uDelayRefreshCount)
		{
			// Delay the refresh in full-screen mode (to allow screen-capabilities to take effect) - required for Win7 (and others?)
			--uDelayRefreshCount;
		}
		else
		{
			int xDst = 0;
			int yDst = 0;

			if (g_bIsFullScreen)
			{
				// Why the need to set the mid-position here, but not for (full-screen) LOGO or DEBUG modes?
				xDst = (g_nDDFullScreenW-W)/2 - VIEWPORTX*2;
				yDst = (g_nDDFullScreenH-H)/2;
			}

			StretchBlt(
				hFrameDC,
				xDst, yDst,				// xDst, yDst
				W, H,					// wDst, hDst
				g_hDeviceDC,
				BORDER_W, BORDER_H,		// xSrc, ySrc
				FRAMEBUFFER_BORDERLESS_W, FRAMEBUFFER_BORDERLESS_H, // wSrc, hSrc
				SRCCOPY );
		}
	}

	GdiFlush();

	FrameReleaseVideoDC();

	g_VideoForceFullRedraw = 0;
// NTSC_END
}

//===========================================================================
void VideoReinitialize ()
{
	NTSC_VideoInitAppleType(g_dwCyclesThisFrame);
	NTSC_SetVideoStyle();
}

//===========================================================================
void VideoResetState ()
{
	g_nAltCharSetOffset    = 0;
	g_uVideoMode           = VF_TEXT;
	g_VideoForceFullRedraw = 1;
}


//===========================================================================

BYTE VideoSetMode (WORD, WORD address, BYTE write, BYTE, ULONG uExecutedCycles)
{
	address &= 0xFF;

//	DWORD oldpage2 = SW_PAGE2;
//	int   oldvalue = g_nAltCharSetOffset+(int)(g_uVideoMode & ~(VF_80STORE | VF_PAGE2));

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

	// Apple IIe, Techical Notes, #3: Double High-Resolution Graphics
	// 80STORE must be OFF to display page 2
	if (SW_80STORE)
		g_uVideoMode &= ~VF_PAGE2;

// NTSC_BEGIN
	NTSC_SetVideoMode( g_uVideoMode );
// NTSC_END

#if 0 // NTSC_CLEANUP: Is this still needed??

	if (oldvalue != g_nAltCharSetOffset+(int)(g_uVideoMode & ~(VF_80STORE | VF_PAGE2)))
		g_VideoForceFullRedraw = 1;	// Defer video redraw until VideoEndOfVideoFrame()

	if (oldpage2 != SW_PAGE2)
	{
		// /g_bVideoUpdatedThisFrame/ is used to limit the video update to once per 60Hz frame (CPU clk=1MHz):
		// . this easily supports the common double-buffered "flip-immediate" case (eg. Airheart flips at a max of ~15Hz, Skyfox/Boulderdash at a max of ~11Hz)
		// . crucially this prevents tight-loop page flipping (GH#129,GH#204) from max'ing out an x86 CPU core (and not providing realtime emulation)
		// NB. Deferring the update by just setting /g_VideoForceFullRedraw/ is not an option, since this doesn't provide "flip-immediate"
		//
		// Ultimately this isn't the correct solution, and proper cycle-accurate video rendering should be done, but this is a much bigger job!
		// TODO-Michael: Is MemReadFloatingBus() still accurate now that we have proper per cycle video rendering??

		if (!g_bVideoUpdatedThisFrame)
		{
			VideoRedrawScreen(); // VideoRefreshScreen();
			g_bVideoUpdatedThisFrame = true;
		}
	}
#endif // NTSC_CLEANUP
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

void VideoSetForceFullRedraw(void)
{
	g_VideoForceFullRedraw = 1;
}

//===========================================================================

void VideoSetSnapshot_v1(const UINT AltCharSet, const UINT VideoMode)
{
	g_nAltCharSetOffset = !AltCharSet ? 0 : 256;
	g_uVideoMode = VideoMode;

// NTSC_BEGIN
	NTSC_SetVideoMode( g_uVideoMode );
// NTSC_END
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

// NTSC_BEGIN
	NTSC_SetVideoMode( g_uVideoMode );
// NTSC_END

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

// Derived from VideoGetScannerAddress()
bool VideoGetVbl(const DWORD uExecutedCycles)
{
    // get video scanner position
    //
    int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

    // calculate video parameters according to display standard
    //
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;

    // calculate vertical scanning state
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;

    // update VBL' state
    //
	if (v_4 & v_3) // VBL?
	{
		return false; // Y: VBL' is false
	}
	else
	{
		return true; // N: VBL' is true
	}
}

//===========================================================================

#define SCREENSHOT_BMP 1
#define SCREENSHOT_TGA 0
	
// alias for nSuffixScreenShotFileName
static int  g_nLastScreenShot = 0;
const  int nMaxScreenShot = 999999999;

static int   g_iScreenshotType;
static char *g_pLastDiskImageName = NULL;

//const  int nMaxScreenShot = 2;

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
void Video_TakeScreenShot( int iScreenShotType )
{
	char sScreenShotFileName[ MAX_PATH ];

	g_iScreenshotType = iScreenShotType;

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

	Video_SaveScreenShot( sScreenShotFileName );
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
void Video_MakeScreenShot(FILE *pFile)
{
	WinBmpHeader_t *pBmp = &g_tBmpHeader;

	Video_SetBitmapHeader(
		pBmp,
		g_iScreenshotType ? FRAMEBUFFER_BORDERLESS_W/2 : FRAMEBUFFER_BORDERLESS_W,
		g_iScreenshotType ? FRAMEBUFFER_BORDERLESS_H/2 : FRAMEBUFFER_BORDERLESS_H,
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
	pSrc += BORDER_H * FRAMEBUFFER_W;	// Skip top border
	pSrc += BORDER_W;					// Skip left border

	if( g_iScreenshotType == SCREENSHOT_280x192 )
	{
		pSrc += FRAMEBUFFER_W;	// Start on odd scanline (otherwise for 50% scanline mode get an all black image!)

		uint32_t  aScanLine[ 280 ];
		uint32_t *pDst;

		// 50% Half Scan Line clears every odd scanline.
		// SHIFT+PrintScreen saves only the even rows.
		// NOTE: Keep in sync with _Video_RedrawScreen() & Video_MakeScreenShot()
		for( int y = 0; y < FRAMEBUFFER_BORDERLESS_H/2; y++ )
		{
			pDst = aScanLine;
			for( int x = 0; x < FRAMEBUFFER_BORDERLESS_W/2; x++ )
			{
				*pDst++ = pSrc[1]; // correction for left edge loss of scaled scanline [Bill Buckel, B#18928]
				pSrc += 2; // skip odd pixels
			}
			fwrite( aScanLine, sizeof(uint32_t), FRAMEBUFFER_BORDERLESS_W/2, pFile );
			pSrc += FRAMEBUFFER_W; // scan lines doubled - skip odd ones
			pSrc += BORDER_W*2;	// Skip right border & next line's left border
		}
	}
	else
	{
		for( int y = 0; y < FRAMEBUFFER_BORDERLESS_H; y++ )
		{
			fwrite( pSrc, sizeof(uint32_t), FRAMEBUFFER_BORDERLESS_W, pFile );
			pSrc += FRAMEBUFFER_W;
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
void Video_SaveScreenShot( const char *pScreenShotFileName )
{
	FILE *pFile = fopen( pScreenShotFileName, "wb" );
	if( pFile )
	{
		Video_MakeScreenShot( pFile );
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
		g_eVideoType = VT_COLOR_STANDARD; // Old default: VT_COLOR_TVEMU
}

void Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE           ),g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES),g_uHalfScanLines);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR     ),g_nMonochromeRGB);
}

// ____________________________________________________________________

//===========================================================================
void videoCreateDIBSection()
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
	ZeroMemory( g_pFramebufferbits, FRAMEBUFFER_W*FRAMEBUFFER_H*4 );

	NTSC_VideoInit( g_pFramebufferbits );
}
