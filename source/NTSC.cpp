/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms
Copyright (C) 2014-2016, Michael Pohoreski, Tom Charlesworth

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

// Includes
	#include "StdAfx.h"
	#include "Applewin.h"
	#include "CPU.h"	// CpuGetCyclesThisVideoFrame()
	#include "Frame.h"
	#include "Memory.h" // MemGetMainPtr() MemGetAuxPtr()
	#include "Video.h"  // g_pFramebufferbits
	#include "RGBMonitor.h"

	#include "NTSC.h"
	#include "NTSC_CharSet.h"


// GH#555: Extend the 14M video modes by 1 pixel
// . 14M (DHGR,DGR,80COL) are shifted right by 1 pixel, so zero out the left-most visible pixel.
// .  7M (all other modes) are not shift right by 1 pixel, so zero out the right-most visible pixel.
// NB. This 1 pixel shift is a workaround for the 14M video modes that actually start 7x 14M pixels to the left on *real h/w*.
// . 7x 14M pixels early + 1x 14M pixel shifted right = 2 complete color phase rotations.
// . ie. the 14M colors are correct, but being 1 pixel out is the closest we can get the 7M and 14M video modes to overlap.
// . The alternative is to render the 14M correctly 7 pixels early, but have 7-pixel borders left (for 7M modes) or right (for 14M modes).
#define EXTEND_14M_VIDEO_BY_1_PIXEL 0

#define NTSC_REMOVE_WHITE_RINGING  1 // 0 = theoritical dimmed white has chroma, 1 = pure white without chroma tinting
#define NTSC_REMOVE_BLACK_GHOSTING 1 // 1 = remove black smear/smudges carrying over
#define NTSC_REMOVE_GRAY_CHROMA    1 // 1 = remove all chroma in gray1 and gray2

#define DEBUG_PHASE_ZERO       0

#define ALT_TABLE 0
#if ALT_TABLE
	#include "ntsc_rgb.h"
#endif

// Defines
	#define HGR_TEST_PATTERN 0

#ifdef _MSC_VER
	#define INLINE __forceinline
#else
	#define INLINE inline
#endif

	#define PI 3.1415926535898f
	#define DEG_TO_RAD(x) (PI*(x)/180.f) // 2PI=360, PI=180,PI/2=90,PI/4=45
	#define RAD_45  PI*0.25f
	#define RAD_90  PI*0.5f
	#define RAD_360 PI*2.f

	// sadly float64 precision is needed
	#define real double

	//#define CYCLESTART (PI/4.f) // PI/4 = 45 degrees
	#define CYCLESTART (DEG_TO_RAD(45))

// Types

	struct ColorSpace_PAL_t // Phase Amplitute Luma
	{
		float phase;
		float amp;
		float luma;
	};

	struct ColorSpace_YIQ_t
	{
		float y, i, q;
	};

	struct rgba_t
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};

	struct abgr_t
	{
		uint8_t a;
		uint8_t b;
		uint8_t g;
		uint8_t r;
	};

	struct ColorSpace_BGRA_t
	{
		union
		{
			uint32_t n;
			bgra_t   bgra;
			rgba_t   rgba;
			abgr_t   abgr;
		};
	};


// Globals (Public) ___________________________________________________
	uint16_t g_nVideoClockVert = 0; // 9-bit: VC VB VA V5 V4 V3 V2 V1 V0 = 0 .. 262
	uint16_t g_nVideoClockHorz = 0; // 6-bit:          H5 H4 H3 H2 H1 H0 = 0 .. 64, 25 >= visible (NB. final hpos is 2 cycles long, so a line is 65 cycles)

// Globals (Private) __________________________________________________
	static int g_nVideoCharSet = 0;
	static int g_nVideoMixed   = 0;
	static int g_nHiresPage    = 1;
	static int g_nTextPage     = 1;

	// Understanding the Apple II, Timing Generation and the Video Scanner, Pg 3-11
	// Vertical Scanning
	// Horizontal Scanning
	// "There are exactly 17030 (65 x 262) 6502 cycles in every television scan of an American Apple."
	#define VIDEO_SCANNER_MAX_HORZ   65 // TODO: use Video.cpp: kHClocks
	#define VIDEO_SCANNER_MAX_VERT  262 // TODO: use Video.cpp: kNTSCScanLines
	static const int VIDEO_SCANNER_6502_CYCLES = VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_MAX_VERT;

	#define VIDEO_SCANNER_HORZ_COLORBURST_BEG 12
	#define VIDEO_SCANNER_HORZ_COLORBURST_END 16

	#define VIDEO_SCANNER_HORZ_START 25 // first displayable horz scanner index
	#define VIDEO_SCANNER_Y_MIXED   160 // num scanlins for mixed graphics + text
	#define VIDEO_SCANNER_Y_DISPLAY 192 // max displayable scanlines

	static bgra_t *g_pVideoAddress = 0;
	static bgra_t *g_pScanLines[VIDEO_SCANNER_Y_DISPLAY*2];  // To maintain the 280x192 aspect ratio for 560px width, we double every scan line -> 560x384

	static const UINT g_kFrameBufferWidth = GetFrameBufferWidth();

	static unsigned short (*g_pHorzClockOffset)[VIDEO_SCANNER_MAX_HORZ] = 0;

	typedef void (*UpdateScreenFunc_t)(long);
	static UpdateScreenFunc_t g_apFuncVideoUpdateScanline[VIDEO_SCANNER_Y_DISPLAY];
	static UpdateScreenFunc_t g_pFuncUpdateTextScreen     = 0; // updateScreenText40;
	static UpdateScreenFunc_t g_pFuncUpdateGraphicsScreen = 0; // updateScreenText40;
	static UpdateScreenFunc_t g_pFuncModeSwitchDelayed = 0;

	typedef void (*UpdatePixelFunc_t)(uint16_t);
	static UpdatePixelFunc_t g_pFuncUpdateBnWPixel = 0; //updatePixelBnWMonitorSingleScanline;
	static UpdatePixelFunc_t g_pFuncUpdateHuePixel = 0; //updatePixelHueMonitorSingleScanline;

	static uint8_t  g_nTextFlashCounter = 0;
	static uint16_t g_nTextFlashMask    = 0;

	static unsigned g_aPixelMaskGR       [ 16];
	static uint16_t g_aPixelDoubleMaskHGR[128]; // hgrbits -> g_aPixelDoubleMaskHGR: 7-bit mono 280 pixels to 560 pixel doubling

	static int g_nLastColumnPixelNTSC;
	static int g_nColorBurstPixels;

	#define INITIAL_COLOR_PHASE 0
	static int g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
	static int g_nSignalBitsNTSC = 0;

	#define NTSC_NUM_PHASES     4
	#define NTSC_NUM_SEQUENCES  4096

	const uint32_t ALPHA32_MASK = 0xFF000000; // Win32: aarrggbb

/*extern*/ uint32_t g_nChromaSize = 0; // for NTSC_VideoGetChromaTable()
	static bgra_t   g_aBnWMonitor                 [NTSC_NUM_SEQUENCES];
	static bgra_t   g_aHueMonitor[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];
	static bgra_t   g_aBnwColorTV                 [NTSC_NUM_SEQUENCES];
	static bgra_t   g_aHueColorTV[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];

	// g_aBnWMonitor * g_nMonochromeRGB -> g_aBnWMonitorCustom
	// g_aBnwColorTV * g_nMonochromeRGB -> g_aBnWColorTVCustom
	static bgra_t g_aBnWMonitorCustom           [NTSC_NUM_SEQUENCES];
	static bgra_t g_aBnWColorTVCustom           [NTSC_NUM_SEQUENCES];

	#define CHROMA_ZEROS 2
	#define CHROMA_POLES 2
	#define CHROMA_GAIN  7.438011255f // Should this be 7.15909 MHz ?
	#define CHROMA_0    -0.7318893645f
	#define CHROMA_1     1.2336442711f

	//#define LUMGAIN  1.062635655e+01
	//#define LUMCOEF1  -0.3412038399
	//#define LUMCOEF2  0.9647813115
	#define LUMA_ZEROS  2
	#define LUMA_POLES  2
	#define LUMA_GAIN  13.71331570f   // Should this be 14.318180 MHz ?
	#define LUMA_0     -0.3961075449f
	#define LUMA_1      1.1044202472f

	#define SIGNAL_ZEROS 2
	#define SIGNAL_POLES 2
	#define SIGNAL_GAIN  7.614490548f  // Should this be 7.15909 MHz ?
	#define SIGNAL_0    -0.2718798058f 
	#define SIGNAL_1     0.7465656072f 

// Tables
	// Video scanner tables are now runtime-generated using UTAIIe logic
	static unsigned short g_aClockVertOffsetsHGR[VIDEO_SCANNER_MAX_VERT];
	static unsigned short g_aClockVertOffsetsTXT[33];
	static unsigned short APPLE_IIP_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ];
	static unsigned short APPLE_IIE_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ];

#ifdef _DEBUG
	static unsigned short g_kClockVertOffsetsHGR[ VIDEO_SCANNER_MAX_VERT ] =
	{
		0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
		0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
		0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
		0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

		0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
		0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
		0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
		0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

		0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
		0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
		0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
		0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

		0x0000,0x0400,0x0800,0x0C00,0x1000,0x1400,0x1800,0x1C00,0x0080,0x0480,0x0880,0x0C80,0x1080,0x1480,0x1880,0x1C80,
		0x0100,0x0500,0x0900,0x0D00,0x1100,0x1500,0x1900,0x1D00,0x0180,0x0580,0x0980,0x0D80,0x1180,0x1580,0x1980,0x1D80,
		0x0200,0x0600,0x0A00,0x0E00,0x1200,0x1600,0x1A00,0x1E00,0x0280,0x0680,0x0A80,0x0E80,0x1280,0x1680,0x1A80,0x1E80,
		0x0300,0x0700,0x0B00,0x0F00,0x1300,0x1700,0x1B00,0x1F00,0x0380,0x0780,0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80,

		0x0B80,0x0F80,0x1380,0x1780,0x1B80,0x1F80
	};

	static unsigned short g_kClockVertOffsetsTXT[33] =
	{
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,

		0x380
	};

	static unsigned short kAPPLE_IIP_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ] =
	{
		{0x1068,0x1068,0x1069,0x106A,0x106B,0x106C,0x106D,0x106E,0x106F,
		 0x1070,0x1071,0x1072,0x1073,0x1074,0x1075,0x1076,0x1077,
		 0x1078,0x1079,0x107A,0x107B,0x107C,0x107D,0x107E,0x107F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
		 0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027},

		{0x1010,0x1010,0x1011,0x1012,0x1013,0x1014,0x1015,0x1016,0x1017,
		 0x1018,0x1019,0x101A,0x101B,0x101C,0x101D,0x101E,0x101F,
		 0x1020,0x1021,0x1022,0x1023,0x1024,0x1025,0x1026,0x1027,
		 0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
		 0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
		 0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
		 0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
		 0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F},

		{0x1038,0x1038,0x1039,0x103A,0x103B,0x103C,0x103D,0x103E,0x103F,
		 0x1040,0x1041,0x1042,0x1043,0x1044,0x1045,0x1046,0x1047,
		 0x1048,0x1049,0x104A,0x104B,0x104C,0x104D,0x104E,0x104F,
		 0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
		 0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
		 0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
		 0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
		 0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077},

		{0x1060,0x1060,0x1061,0x1062,0x1063,0x1064,0x1065,0x1066,0x1067,
		 0x1068,0x1069,0x106A,0x106B,0x106C,0x106D,0x106E,0x106F,
		 0x1070,0x1071,0x1072,0x1073,0x1074,0x1075,0x1076,0x1077,
		 0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F},

		{0x1060,0x1060,0x1061,0x1062,0x1063,0x1064,0x1065,0x1066,0x1067,
		 0x1068,0x1069,0x106A,0x106B,0x106C,0x106D,0x106E,0x106F,
		 0x1070,0x1071,0x1072,0x1073,0x1074,0x1075,0x1076,0x1077,
		 0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F}
	};

	static unsigned short kAPPLE_IIE_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ] =
	{
		{0x0068,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
		 0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
		 0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
		 0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027},

		{0x0010,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
		 0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
		 0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
		 0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
		 0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
		 0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
		 0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F},

		{0x0038,0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
		 0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
		 0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
		 0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
		 0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
		 0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
		 0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
		 0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077},

		{0x0060,0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
		 0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
		 0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
		 0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F},

		{0x0060,0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
		 0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
		 0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
		 0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
		 0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
		 0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
		 0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
		 0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F}
	};
#endif

	/*
		http://www.kreativekorp.com/miscpages/a2info/munafo.shtml

		"Primary" lo-res colors
		Color      GR        Duty cycle  Phase
		======================================
		Red        COLOR=1    45 to 135    90
		Dark-blue  COLOR=2   315 to 45      0
		Dark-green COLOR=4   225 to 315   270
		Brown      COLOR=8   135 to 225   180
	*/
	ColorSpace_PAL_t aPaletteYIQ[ 16 ] =
	{                   // Lo Hi Dh
		 {  0,  0,  0 } //  0  0     Black
		,{ 90, 60, 25 } //  1     1  Red
		,{  0, 60, 25 } //  2     8  Dark Blue
		,{ 45,100, 50 } //  3  2  9  Purple
		,{270, 60, 25 } //  4        Dark Green
		,{  0,  0, 50 } //  5        Grey
		,{315,100, 50 } //  6        Medium Blue
		,{  0, 60, 75 } //  7        Light Blue
		,{180, 60, 25 } //  8        Brown
		,{135,100, 50 } //  9        Orange
		,{  0,  0, 50 } // 10
		,{ 90, 60, 75 } // 11        Pink
		,{225,100, 50 } // 12        Light Green
		,{180, 60, 75 } // 13        Yellow
		,{270,  60, 75} // 14        Aqua
		,{  0,  0,100 } // 15        White
	};

// purple   HCOLOR=2  45 100   50    255  68 253
// orange   HCOLOR=5 135 100   50    255 106  60
// green    HCOLOR=1 225 100   50     20 245  60
// blue     HCOLOR=6 315 100   50     20 207 253

	rgba_t aPaletteRGB[ 16 ] =
	{
		 {   0,   0,   0 } //  0
		,{ 227,  30,  96 } //  1
		,{  96,  78, 189 } //  2
		,{ 255,  68, 253 } //  3
		,{   0, 163,  96 } //  4
		,{ 156, 156, 156 } //  5
		,{  20, 207, 253 } //  6
		,{ 208, 195, 255 } //  7
		,{  96, 114,   3 } //  8
		,{ 255, 106,  60 } //  9
		,{ 156, 156, 156 } // 10
		,{ 255, 160, 208 } // 11
		,{  20, 245,  60 } // 12
		,{ 208, 221, 141 } // 13
		,{ 114, 255, 208 } // 14
		,{ 255, 255, 255 } // 15
	};

	static csbits_t csbits;		// charset, optionally followed by alt charset

// Prototypes
	INLINE float     clampZeroOne( const float & x );
	INLINE uint8_t   getCharSetBits( const int iChar );
	INLINE uint16_t  getLoResBits( uint8_t iByte );
	INLINE uint32_t  getScanlineColor( const uint16_t signal, const bgra_t *pTable );
	INLINE uint32_t* getScanlineNext1Address();
	INLINE uint32_t* getScanlinePrev1Address();
	INLINE uint32_t* getScanlinePrev2Address();
	INLINE uint32_t* getScanlineThis0Address();
	INLINE void      updateColorPhase();
	INLINE void      updateFlashRate();
	INLINE void      updateFramebufferColorTVSingleScanline( uint16_t signal, bgra_t *pTable );
	INLINE void      updateFramebufferColorTVDoubleScanline( uint16_t signal, bgra_t *pTable );
	INLINE void      updateFramebufferMonitorSingleScanline( uint16_t signal, bgra_t *pTable );
	INLINE void      updateFramebufferMonitorDoubleScanline( uint16_t signal, bgra_t *pTable );
	INLINE void      updatePixels( uint16_t bits );
	INLINE void      updateVideoScannerHorzEOL();
	INLINE void      updateVideoScannerAddress();
	INLINE uint16_t  getVideoScannerAddressTXT();
	INLINE uint16_t  getVideoScannerAddressHGR();

	static void initChromaPhaseTables();
	static real initFilterChroma   (real z);
	static real initFilterLuma0    (real z);
	static real initFilterLuma1    (real z);
	static real initFilterSignal(real z);
	static void initPixelDoubleMasks(void);
	static void updateMonochromeTables( uint16_t r, uint16_t g, uint16_t b );

	static void updatePixelBnWColorTVSingleScanline( uint16_t compositeSignal );
	static void updatePixelBnWColorTVDoubleScanline( uint16_t compositeSignal );
	static void updatePixelBnWMonitorSingleScanline( uint16_t compositeSignal );
	static void updatePixelBnWMonitorDoubleScanline( uint16_t compositeSignal );
	static void updatePixelHueColorTVSingleScanline( uint16_t compositeSignal );
	static void updatePixelHueColorTVDoubleScanline( uint16_t compositeSignal );
	static void updatePixelHueMonitorSingleScanline( uint16_t compositeSignal );
	static void updatePixelHueMonitorDoubleScanline( uint16_t compositeSignal );

	static void updateScreenDoubleHires40( long cycles6502 );
	static void updateScreenDoubleHires80( long cycles6502 );
	static void updateScreenDoubleLores40( long cycles6502 );
	static void updateScreenDoubleLores80( long cycles6502 );
	static void updateScreenSingleHires40( long cycles6502 );
	static void updateScreenSingleLores40( long cycles6502 );
	static void updateScreenText40       ( long cycles6502 );
	static void updateScreenText80       ( long cycles6502 );

//===========================================================================
// NB. This func only exists so that EXTEND_14M_VIDEO_BY_1_PIXEL only needs to exist in this cpp file!
UINT NTSC_GetFrameBufferBorderlessWidth(void)
{
#if !EXTEND_14M_VIDEO_BY_1_PIXEL
	return 560;	// 560 = Double Hi-Res
#else
	return 561;	// 560 = Double Hi-Res, +1 for GH#555
#endif
}

//===========================================================================
static void set_csbits()
{
	// NB. For models that don't have an alt charset then set /g_nVideoCharSet/ to zero
	switch ( GetApple2Type() )
	{
	case A2TYPE_APPLE2:			csbits = &csbits_a2[0];         g_nVideoCharSet = 0; break;
	case A2TYPE_APPLE2PLUS:		csbits = &csbits_a2[0];         g_nVideoCharSet = 0; break;
	case A2TYPE_APPLE2E:		csbits = Get2e_csbits();		break;
	case A2TYPE_APPLE2EENHANCED:csbits = Get2e_csbits();		break;
	case A2TYPE_PRAVETS82:	    csbits = &csbits_pravets82[0];  g_nVideoCharSet = 0; break;	// Apple ][ clone
	case A2TYPE_PRAVETS8M:	    csbits = &csbits_pravets8M[0];  g_nVideoCharSet = 0; break;	// Apple ][ clone
	case A2TYPE_PRAVETS8A:	    csbits = &csbits_pravets8C[0];  break;	// Apple //e clone
	case A2TYPE_TK30002E:		csbits = &csbits_enhanced2e[0]; break;	// Enhanced Apple //e clone
	default:					csbits = &csbits_enhanced2e[0]; break;
	}
}

//===========================================================================
inline float clampZeroOne( const float & x )
{
	if (x < 0.f) return 0.f;
	if (x > 1.f) return 1.f;
	/* ...... */ return x;
}

//===========================================================================
inline uint8_t getCharSetBits(int iChar)
{
	return csbits[g_nVideoCharSet][iChar][g_nVideoClockVert & 7];
}

//===========================================================================
inline uint16_t getLoResBits( uint8_t iByte )
{
	return g_aPixelMaskGR[ (iByte >> (g_nVideoClockVert & 4)) & 0xF ]; 
}

//===========================================================================
inline uint32_t getScanlineColor( const uint16_t signal, const bgra_t *pTable )
{
	g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; // 12-bit
	return *(uint32_t*) &pTable[ g_nSignalBitsNTSC ];
}

//===========================================================================
inline uint32_t* getScanlineNext1Address()
{
	return (uint32_t*) (g_pVideoAddress - 1*g_kFrameBufferWidth);
}

//===========================================================================
inline uint32_t* getScanlinePrev1Address()
{
	return (uint32_t*) (g_pVideoAddress + 1*g_kFrameBufferWidth);
}

//===========================================================================
inline uint32_t* getScanlinePrev2Address()
{
	return (uint32_t*) (g_pVideoAddress + 2*g_kFrameBufferWidth);
}

//===========================================================================
inline uint32_t* getScanlineThis0Address()
{
	return (uint32_t*) g_pVideoAddress;
}

//===========================================================================
inline void updateColorPhase()
{
	g_nColorPhaseNTSC++;
	g_nColorPhaseNTSC &= 3;
}

//===========================================================================
inline void updateFlashRate() // TODO: Flash rate should be constant (regardless of CPU speed)
{
	// BUG: In unthrottled CPU mode, flash rate should not be affected

	// Flash rate:
	// . NTSC : 60/16 ~= 4Hz
	// . PAL  : 50/16 ~= 3Hz
	if ((++g_nTextFlashCounter & 0xF) == 0)
		g_nTextFlashMask ^= -1; // 16-bits

	// The old way to handle flashing was
	//     if ((SW_TEXT || SW_MIXED) ) // && !SW_80COL) // FIX: FLASH 80-Column
	//	       g_nTextFlashMask = true;
	// The new way is to check the active char set, inlined:
	//     if (0 == g_nVideoCharSet && 0x40 == (m & 0xC0)) // Flash only if mousetext not active
}

#if 0
#define updateFramebufferMonitorSingleScanline(signal,table) \
	do { \
		uint32_t *cp, *mp; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		cp = (uint32_t*) &table[g_nSignalBitsNTSC]; \
		*(uint32_t*)g_pVideoAddress = *cp; \
		mp = (uint32_t*)(g_pVideoAddress - FRAMEBUFFER_W); \
		*mp = ((*cp & 0x00fcfcfc) >> 2) | ALPHA32_MASK; \
		g_pVideoAddress++; \
	} while(0)

// prevp is never used nor blended with!
#define updateFramebufferColorTVSingleScanline(signal,table) \
	do { \
		uint32_t ntscp, prevp, betwp; \
		uint32_t *prevlin, *between; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		prevlin = (uint32_t*)(g_pVideoAddress + 2*FRAMEBUFFER_W); \
		between = (uint32_t*)(g_pVideoAddress + 1*FRAMEBUFFER_W); \
		ntscp = *(uint32_t*) &table[g_nSignalBitsNTSC]; /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ntscp - ((ntscp & 0x00fcfcfc) >> 2); \
		*between = betwp | ALPHA32_MASK; \
		*(uint32_t*)g_pVideoAddress = ntscp; \
		g_pVideoAddress++; \
	} while(0)

#define updateFramebufferMonitorDoubleScanline(signal,table) \
	do { \
		uint32_t *cp, *mp; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		cp = (uint32_t*) &table[g_nSignalBitsNTSC]; \
		mp = (uint32_t*)(g_pVideoAddress - FRAMEBUFFER_W); \
		*(uint32_t*)g_pVideoAddress = *mp = *cp; \
		g_pVideoAddress++; \
	} while(0)

#define updateFramebufferColorTVDoubleScanline(signal,table) \
	do { \
		uint32_t ntscp, prevp, betwp; \
		uint32_t *prevlin, *between; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		prevlin = (uint32_t*)(g_pVideoAddress + 2*FRAMEBUFFER_W); \
		between = (uint32_t*)(g_pVideoAddress + 1*FRAMEBUFFER_W); \
		ntscp = *(uint32_t*) &table[g_nSignalBitsNTSC]; /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ((ntscp & 0x00fefefe) >> 1) + ((prevp & 0x00fefefe) >> 1); \
		*between = betwp | ALPHA32_MASK; \
		*(uint32_t*)g_pVideoAddress = ntscp; \
		g_pVideoAddress++; \
	} while(0)
#else

//===========================================================================
inline void updateFramebufferColorTVSingleScanline( uint16_t signal, bgra_t *pTable )
{
	/* */ uint32_t *pLine0Address = getScanlineThis0Address();
	/* */ uint32_t *pLine1Address = getScanlinePrev1Address();
	/* */ uint32_t *pLine2Address = getScanlinePrev2Address();

	const uint32_t color0 = getScanlineColor( signal, pTable );
	const uint32_t color2 = *pLine2Address;
//	const uint32_t color1 = color0 - ((color2 & 0x00fcfcfc) >> 2); // BUG? color0 - color0? not color0-color2?
	// TC: The above operation "color0 - ((color2 & 0x00fcfcfc) >> 2)" causes underflow, so I've recoded to clamp on underflow:
	uint32_t color1;
	{
		int r=(color0>>16)&0xff, g=(color0>>8)&0xff, b=color0&0xff;
		uint32_t color2_prime = (color2 & 0x00fcfcfc) >> 2;
		r -= (color2_prime>>16)&0xff; if (r<0) r=0;	// clamp to 0 on underflow
		g -= (color2_prime>>8)&0xff;  if (g<0) g=0;	// clamp to 0 on underflow
		b -= (color2_prime)&0xff;     if (b<0) b=0;	// clamp to 0 on underflow
		color1 = (r<<16)|(g<<8)|(b);
	}

	/* */  *pLine1Address = color1 | ALPHA32_MASK;
	/* */  *pLine0Address = color0;
	/* */ g_pVideoAddress++;
}

//===========================================================================
inline void updateFramebufferColorTVDoubleScanline( uint16_t signal, bgra_t *pTable )
{
	/* */ uint32_t *pLine0Address = getScanlineThis0Address();
	/* */ uint32_t *pLine1Address = getScanlinePrev1Address();
	const uint32_t *pLine2Address = getScanlinePrev2Address();

	const uint32_t color0 = getScanlineColor( signal, pTable );
	const uint32_t color2 = *pLine2Address;
	const uint32_t color1 = ((color0 & 0x00fefefe) >> 1) + ((color2 & 0x00fefefe) >> 1); // 50% Blend

	/* */  *pLine1Address = color1 | ALPHA32_MASK;
	/* */  *pLine0Address = color0;
	/* */ g_pVideoAddress++;
}

//===========================================================================
inline void updateFramebufferMonitorSingleScanline( uint16_t signal, bgra_t *pTable )
{
	/* */ uint32_t *pLine0Address = getScanlineThis0Address();
	/* */ uint32_t *pLine1Address = getScanlineNext1Address();
	const uint32_t color0 = getScanlineColor( signal, pTable );
	const uint32_t color1 = ((color0 & 0x00fcfcfc) >> 2); // 25% Blend (original)
//	const uint32_t color1 = ((color0 & 0x00fefefe) >> 1); // 50% Blend -- looks OK most of the time; Archon looks poor

	/* */  *pLine1Address = color1 | ALPHA32_MASK;
	/* */  *pLine0Address = color0;
	/* */ g_pVideoAddress++;
}

//===========================================================================
inline void updateFramebufferMonitorDoubleScanline( uint16_t signal, bgra_t *pTable )
{
	/* */ uint32_t *pLine0Address = getScanlineThis0Address();
	/* */ uint32_t *pLine1Address = getScanlineNext1Address();
	const uint32_t color0 = getScanlineColor( signal, pTable );

	/* */  *pLine1Address = color0;
	/* */  *pLine0Address = color0;
	/* */ g_pVideoAddress++;
}
#endif

//===========================================================================
inline bool GetColorBurst( void )
{
	return g_nColorBurstPixels >= 2;
}

//===========================================================================

void update7MonoPixels( uint16_t bits )
{
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
	g_pFuncUpdateBnWPixel(bits & 1);
}

//===========================================================================

// NB. g_nLastColumnPixelNTSC = bits.b13 will be superseded by these parent funcs which use bits.b14:
// . updateScreenDoubleHires80(), updateScreenDoubleLores80(), updateScreenText80()
inline void updatePixels( uint16_t bits )
{
	if (!GetColorBurst())
	{ 
		/* #1 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #2 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #3 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #4 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #5 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #6 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		/* #7 of 7 */
		g_pFuncUpdateBnWPixel(bits & 1); bits >>= 1;
		g_pFuncUpdateBnWPixel(bits & 1);
        g_nLastColumnPixelNTSC = bits & 1;
	}
	else
	{
		/* #1 of 7 */                                // abcd efgh ijkl mnop
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0abc defg hijk lmno
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 00ab cdef ghi jklmn
		/* #2 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 000a bcde fghi jklm
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 abcd efgh ijkl
		/* #3 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0abc defg hijk
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 00ab cdef ghij
		/* #4 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 000a bcde fghi
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 abcd efgh
		/* #5 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 0abc defg
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 00ab cdef
		/* #6 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 000a bcde
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 0000 abcd
		/* #7 of 7 */
		g_pFuncUpdateHuePixel(bits & 1); bits >>= 1; // 0000 0000 0000 0abc
		g_pFuncUpdateHuePixel(bits & 1);           
        g_nLastColumnPixelNTSC = bits & 1;
	}
}

//===========================================================================

inline void updateVideoScannerHorzEOLSimple()
{
	if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
	{
		g_nVideoClockHorz = 0;

		if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
		{
			g_nVideoClockVert = 0;

			updateFlashRate();
		}

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			updateVideoScannerAddress();
		}
	}
}

#if !EXTEND_14M_VIDEO_BY_1_PIXEL
// NOTE: This writes out-of-bounds for a 560x384 framebuffer
inline void updateVideoScannerHorzEOL()
{
	if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
	{
		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (!GetColorBurst())
			{
				// Only for: VF_TEXT && !VF_MIXED (ie. full 24-row TEXT40 or TEXT80)
				g_pFuncUpdateBnWPixel(g_nLastColumnPixelNTSC);
				g_pFuncUpdateBnWPixel(0);
				g_pFuncUpdateBnWPixel(0);
			}
			else
			{
				g_pFuncUpdateHuePixel(g_nLastColumnPixelNTSC);
				g_pFuncUpdateHuePixel(0);
				g_pFuncUpdateHuePixel(0);
			}
		}

		g_nVideoClockHorz = 0;

		if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
		{
			g_nVideoClockVert = 0;

			updateFlashRate();
		}

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			updateVideoScannerAddress();
		}
	}
}
#endif

//-------------------------------------

#if EXTEND_14M_VIDEO_BY_1_PIXEL
// NB. Only needed for video modes that are 14M and shift the color phase, ie:
// . updateScreenDoubleHires80(), updateScreenDoubleLores80(), updateScreenText80()
// NOTE: This writes out-of-bounds for a 560x384 framebuffer
inline void updateVideoScannerHorzEOL_14M()
{
	if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
	{
		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (!GetColorBurst())
			{
				// Only for: VF_TEXT && !VF_MIXED (ie. full 24-row TEXT40 or TEXT80)
				g_pFuncUpdateBnWPixel(g_nLastColumnPixelNTSC);	// 14M: Output a 561st dot
				g_pFuncUpdateBnWPixel(0);
				g_pFuncUpdateBnWPixel(0);
			}
			else
			{
				g_pFuncUpdateHuePixel(g_nLastColumnPixelNTSC);	// 14M: Output a 561st dot
				g_pFuncUpdateHuePixel(0);
				g_pFuncUpdateHuePixel(0);
			}
		}

		g_nVideoClockHorz = 0;

		if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
		{
			g_nVideoClockVert = 0;

			updateFlashRate();
		}

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			updateVideoScannerAddress();
		}
	}
}

//-----------------

// NOTE: This writes out-of-bounds for a 560x384 framebuffer
inline void updateVideoScannerHorzEOL()
{
	if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
	{
		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ( !GetColorBurst() ||	// Only for: VF_TEXT && !VF_MIXED (ie. full 24-row TEXT40 or TEXT80)
				 (g_eVideoType == VT_MONO_CUSTOM) || (g_eVideoType == VT_MONO_AMBER) || (g_eVideoType == VT_MONO_GREEN) || (g_eVideoType == VT_MONO_WHITE) )
			{
				g_pFuncUpdateBnWPixel(0);
				g_pFuncUpdateBnWPixel(0);

				// 7M: Stop outputting video after 560 dots
				*(UINT32*)&g_pVideoAddress[0] = 0;
				*(UINT32*)&g_pVideoAddress[g_kFrameBufferWidth] = 0;
			}
			else
			{
				g_pFuncUpdateHuePixel(g_nLastColumnPixelNTSC);
				g_pFuncUpdateHuePixel(0);

				// 7M: Stop outputting video after 560 dots
				*(UINT32*)&g_pVideoAddress[0] = 0;
				*(UINT32*)&g_pVideoAddress[g_kFrameBufferWidth] = 0;
			}
		}

		g_nVideoClockHorz = 0;

		if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
		{
			g_nVideoClockVert = 0;

			updateFlashRate();
		}

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			updateVideoScannerAddress();
		}
	}
}
#endif

//===========================================================================
inline void updateVideoScannerAddress()
{
	g_pVideoAddress = g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY ? g_pScanLines[2*g_nVideoClockVert] : g_pScanLines[0];

	// Adjust, as these video styles have 2x 14M pixels of pre-render
	// NB. For VT_COLOR_MONITOR_NTSC, also check color-burst so that TEXT and MIXED(HGR+TEXT) render the TEXT at the same offset (GH#341)
	if (g_eVideoType == VT_MONO_TV || g_eVideoType == VT_COLOR_TV || (g_eVideoType == VT_COLOR_MONITOR_NTSC && GetColorBurst()))
		g_pVideoAddress -= 2;

	// GH#555: For the 14M video modes (DHGR,DGR,80COL), start rendering 1x 14M pixel early to account for these video modes being shifted right by 1 pixel
	// NB. This 1 pixel shift right is a workaround for the 14M video modes that actually start 7x 14M pixels to the left on *real h/w*.
	// . 7x 14M pixels early + 1x 14M pixel shifted right = 2 complete color phase rotations.
	// . ie. the 14M colors are correct, but being 1 pixel out is the closest we can get the 7M and 14M video modes to overlap.
	// . The alternative is to render the 14M correctly 7 pixels early, but have 7-pixel borders left (for 7M modes) or right (for 14M modes).
	if (((g_pFuncUpdateGraphicsScreen == updateScreenDoubleHires80) ||
		(g_pFuncUpdateGraphicsScreen == updateScreenDoubleLores80) ||
		(g_pFuncUpdateGraphicsScreen == updateScreenText80) ||
		(g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED && g_pFuncUpdateTextScreen == updateScreenText80))
		&& (g_eVideoType != VT_COLOR_MONITOR_RGB))	// Fix for "Ansi Story" (Turn the disk over) - Top row of TEXT80 is shifted by 1 pixel
	{
		g_pVideoAddress -= 1;
	}

	g_nColorPhaseNTSC      = INITIAL_COLOR_PHASE;
	g_nLastColumnPixelNTSC = 0;
	g_nSignalBitsNTSC      = 0;
}

//===========================================================================
INLINE uint16_t getVideoScannerAddressTXT()
{
	return (g_aClockVertOffsetsTXT[g_nVideoClockVert/8] + 
		g_pHorzClockOffset         [g_nVideoClockVert/64][g_nVideoClockHorz] + (g_nTextPage  *  0x400));
}

//===========================================================================
INLINE uint16_t getVideoScannerAddressHGR()
{
	// NB. For both A2 and //e use APPLE_IIE_HORZ_CLOCK_OFFSET - see VideoGetScannerAddress() where only TEXT mode adds $1000
	return (g_aClockVertOffsetsHGR[g_nVideoClockVert  ] + 
		APPLE_IIE_HORZ_CLOCK_OFFSET[g_nVideoClockVert/64][g_nVideoClockHorz] + (g_nHiresPage * 0x2000));
}


// Non-Inline _________________________________________________________

// Build the 4 phase chroma lookup table
// The YI'Q' colors are hard-coded
//===========================================================================
static void initChromaPhaseTables (void)
{
	int phase,s,t,n;
	real z,y0,y1,c,i,q;
	real phi,zz;
	float brightness;
	double r64,g64,b64;
	float  r32,g32,b32;	

	for (phase = 0; phase < 4; ++phase)
	{
		phi = (phase * RAD_90) + CYCLESTART;
		for (s = 0; s < NTSC_NUM_SEQUENCES; ++s)
		{
			t = s;
			y0 = y1 = c = i = q = 0.0;

			for (n = 0; n < 12; ++n)
			{
				z = (real)(0 != (t & 0x800));
				t = t << 1;

				for(int k = 0; k < 2; k++ )
				{
					//z = z * 1.25;
					zz = initFilterSignal(z);
					c  = initFilterChroma(zz); // "Mostly" correct _if_ CYCLESTART = PI/4 = 45 degrees
					y0 = initFilterLuma0 (zz);
					y1 = initFilterLuma1 (zz - c);

					c = c * 2.f;
					i = i + (c * cos(phi) - i) / 8.f;
					q = q + (c * sin(phi) - q) / 8.f;

					phi += RAD_45;
				} // k
			} // samples

			brightness = clampZeroOne( (float)z );
			g_aBnWMonitor[s].b = (uint8_t)(brightness * 255);
			g_aBnWMonitor[s].g = (uint8_t)(brightness * 255);
			g_aBnWMonitor[s].r = (uint8_t)(brightness * 255);
			g_aBnWMonitor[s].a = 255;

			brightness = clampZeroOne( (float)y1);
			g_aBnwColorTV[s].b = (uint8_t)(brightness * 255);
			g_aBnwColorTV[s].g = (uint8_t)(brightness * 255);
			g_aBnwColorTV[s].r = (uint8_t)(brightness * 255);
			g_aBnwColorTV[s].a = 255;
			
			/*
				YI'V' to RGB

				[r g b] = [y i v][ 1      1      1    ]
				                 [0.956  -0.272 -1.105]
				                 [0.621  -0.647  1.702]

				[r]   [1   0.956  0.621][y]    
				[g] = [1  -0.272 -0.647][i]
				[b]   [1  -1.105  1.702][v]
			*/
			#define I_TO_R  0.956f
			#define I_TO_G -0.272f
			#define I_TO_B -1.105f

			#define Q_TO_R  0.621f
			#define Q_TO_G -0.647f
			#define Q_TO_B  1.702f

			r64 = y0 + (I_TO_R * i) + (Q_TO_R * q);
			g64 = y0 + (I_TO_G * i) + (Q_TO_G * q);
			b64 = y0 + (I_TO_B * i) + (Q_TO_B * q);

			b32 = clampZeroOne( (float)b64);
			g32 = clampZeroOne( (float)g64);
			r32 = clampZeroOne( (float)r64);

			int color = s & 15;

#if NTSC_REMOVE_WHITE_RINGING
			if( color == 15 ) // white
			{
				r32 = 1;
				g32 = 1;
				b32 = 1;
			}
#endif			

#if NTSC_REMOVE_BLACK_GHOSTING
			if( color == 0 ) // Black
			{
				r32 = 0;
				g32 = 0;
				b32 = 0;
			}
#endif

#if NTSC_REMOVE_GRAY_CHROMA
			if( color == 5 ) // Gray1 & Gray2
			{
				const float g = (float) 0x83 / (float) 0xFF;
				r32 = g;
				g32 = g;
				b32 = g;
			}

			if( color == 10 ) // Gray2 & Gray1
			{
				const float g = (float) 0x78 / (float) 0xFF;
				r32 = g;
				g32 = g;
				b32 = g;
			}
#endif

			g_aHueMonitor[phase][s].b = (uint8_t)(b32 * 255);
			g_aHueMonitor[phase][s].g = (uint8_t)(g32 * 255);
			g_aHueMonitor[phase][s].r = (uint8_t)(r32 * 255);
			g_aHueMonitor[phase][s].a = 255;

			r64 = y1 + (I_TO_R * i) + (Q_TO_R * q);
			g64 = y1 + (I_TO_G * i) + (Q_TO_G * q);
			b64 = y1 + (I_TO_B * i) + (Q_TO_B * q);

			b32 = clampZeroOne( (float)b64 );
			g32 = clampZeroOne( (float)g64 );
			r32 = clampZeroOne( (float)r64 );

#if NTSC_REMOVE_WHITE_RINGING
			if( color == 15 ) // white
			{
				r32 = 1;
				g32 = 1;
				b32 = 1;
			}
#endif			

#if NTSC_REMOVE_BLACK_GHOSTING
			if( color == 0 ) // Black
			{
				r32 = 0;
				g32 = 0;
				b32 = 0;
			}
#endif

			g_aHueColorTV[phase][s].b = (uint8_t)(b32 * 255);
			g_aHueColorTV[phase][s].g = (uint8_t)(g32 * 255);
			g_aHueColorTV[phase][s].r = (uint8_t)(r32 * 255);
			g_aHueColorTV[phase][s].a = 255;
		}
	}

#if DEBUG_PHASE_ZERO
	uint8_t *p = (uint8_t*)g_aHueMonitor;
	*p++ = 0xFF;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0xFF;
#endif

}

/*
http://www-users.cs.york.ac.uk/~fisher/mkfilter/trad.html
Sample Rate: ???
Corner Freq 1: ?
Corner Freq 2: ?

double ButterworthLowPass2( double a, double b, double g, double z )
{
	const  int      POLES=2;
	static double x[POLES+1];
	static double y[POLES+1];

	for( int iPole = 0; iPole < POLES; iPole++ )
	{
		x[iPole] = x[iPole+1];
		y[iPole] = y[iPole+1];
	}

	x[POLES] = z / g;
	y[POLES] = x[0] + x[2] + (2.f*x[1]) + (a*y[0]) + (b*y[1]);

	return y[2];
}

*/

// What filter is this ??
// Filter Order: 2 -> poles for low pass
//===========================================================================
static real initFilterChroma (real z)
{
	static real x[CHROMA_ZEROS + 1] = {0,0,0};
	static real y[CHROMA_POLES + 1] = {0,0,0};

	x[0] = x[1];   x[1] = x[2];   x[2] = z / CHROMA_GAIN;
	y[0] = y[1];   y[1] = y[2];   y[2] = -x[0] + x[2] + (CHROMA_0*y[0]) + (CHROMA_1*y[1]); // inverted x[0]

	return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
//===========================================================================
static real initFilterLuma0 (real z)
{
	static real x[LUMA_ZEROS + 1] = { 0,0,0 };
	static real y[LUMA_POLES + 1] = { 0,0,0 };

	x[0] = x[1];   x[1] = x[2];   x[2] = z / LUMA_GAIN;
	y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (LUMA_0*y[0]) + (LUMA_1*y[1]);

	return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
//===========================================================================
static real initFilterLuma1 (real z)
{
	static real x[LUMA_ZEROS + 1] = { 0,0,0};
	static real y[LUMA_POLES + 1] = { 0,0,0};

	x[0] = x[1];   x[1] = x[2];   x[2] = z / LUMA_GAIN;
	y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (LUMA_0*y[0]) + (LUMA_1*y[1]);

	return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
//===========================================================================
static real initFilterSignal (real z)
{
	static real x[SIGNAL_ZEROS + 1] = { 0,0,0 };
	static real y[SIGNAL_POLES + 1] = { 0,0,0 };

	x[0] = x[1];   x[1] = x[2];   x[2] = z / SIGNAL_GAIN;
	y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (SIGNAL_0*y[0]) + (SIGNAL_1*y[1]);

	return y[2];
}

//===========================================================================
static void initPixelDoubleMasks (void)
{
	/*
		Convert 7-bit monochrome luminance to 14-bit double pixel luminance
		Chroma will be applied later based on the color phase in updatePixelHueMonitorDoubleScanline( luminanceBit )
			0x001 -> 0x0003
			0x002 -> 0x000C
			0x004 -> 0x0030
			0x008 -> 0x00C0
			:     -> :
			0x100 -> 0x4000
	*/
	for (uint8_t byte = 0; byte < 0x80; byte++ ) // Optimization: hgrbits second 128 entries are mirror of first 128
		for (uint8_t bits = 0; bits < 7; bits++ ) // high bit = half pixel shift; pre-optimization: bits < 8
			if (byte & (1 << bits)) // pow2 mask
				g_aPixelDoubleMaskHGR[byte] |= 3 << (bits*2);

	for ( uint16_t color = 0; color < 16; color++ )
		g_aPixelMaskGR[ color ] = (color << 12) | (color << 8) | (color << 4) | (color << 0);
}

//===========================================================================
void updateMonochromeTables( uint16_t r, uint16_t g, uint16_t b )
{
	for( int iSample = 0; iSample < NTSC_NUM_SEQUENCES; iSample++ )
	{
		g_aBnWMonitorCustom[ iSample ].b = (g_aBnWMonitor[ iSample ].b * b) >> 8;
		g_aBnWMonitorCustom[ iSample ].g = (g_aBnWMonitor[ iSample ].g * g) >> 8;
		g_aBnWMonitorCustom[ iSample ].r = (g_aBnWMonitor[ iSample ].r * r) >> 8;
		g_aBnWMonitorCustom[ iSample ].a = 0xFF;

		g_aBnWColorTVCustom[ iSample ].b = (g_aBnwColorTV[ iSample ].b * b) >> 8;
		g_aBnWColorTVCustom[ iSample ].g = (g_aBnwColorTV[ iSample ].g * g) >> 8;
		g_aBnWColorTVCustom[ iSample ].r = (g_aBnwColorTV[ iSample ].r * r) >> 8;
		g_aBnWColorTVCustom[ iSample ].a = 0xFF;
	}
}

//===========================================================================
static void updatePixelBnWMonitorSingleScanline (uint16_t compositeSignal)
{
	updateFramebufferMonitorSingleScanline(compositeSignal, g_aBnWMonitorCustom);
}

//===========================================================================
static void updatePixelBnWMonitorDoubleScanline (uint16_t compositeSignal)
{
	updateFramebufferMonitorDoubleScanline(compositeSignal, g_aBnWMonitorCustom);
}

//===========================================================================
static void updatePixelBnWColorTVSingleScanline (uint16_t compositeSignal)
{
	updateFramebufferColorTVSingleScanline(compositeSignal, g_aBnWColorTVCustom);
}

//===========================================================================
static void updatePixelBnWColorTVDoubleScanline (uint16_t compositeSignal)
{
	updateFramebufferColorTVDoubleScanline(compositeSignal, g_aBnWColorTVCustom);
}

//===========================================================================
static void updatePixelHueColorTVSingleScanline (uint16_t compositeSignal)
{
	updateFramebufferColorTVSingleScanline(compositeSignal, g_aHueColorTV[g_nColorPhaseNTSC]);
	updateColorPhase();
}

//===========================================================================
static void updatePixelHueColorTVDoubleScanline (uint16_t compositeSignal)
{
	updateFramebufferColorTVDoubleScanline(compositeSignal, g_aHueColorTV[g_nColorPhaseNTSC]);
	updateColorPhase();
}

//===========================================================================
static void updatePixelHueMonitorSingleScanline (uint16_t compositeSignal)
{
	updateFramebufferMonitorSingleScanline(compositeSignal, g_aHueMonitor[g_nColorPhaseNTSC]);
	updateColorPhase();
}

//===========================================================================
static void updatePixelHueMonitorDoubleScanline (uint16_t compositeSignal)
{
	updateFramebufferMonitorDoubleScanline(compositeSignal, g_aHueMonitor[g_nColorPhaseNTSC]);
	updateColorPhase();
}

//===========================================================================

#if EXTEND_14M_VIDEO_BY_1_PIXEL
// NB. Only needed for video modes that are 14M and shift the color phase, ie:
// . updateScreenDoubleHires80(), updateScreenDoubleLores80(), updateScreenText80()
inline void zeroPixel0_14M(void)	// GH#555
{
	if (g_nVideoClockHorz == VIDEO_SCANNER_HORZ_START)
	{
		UINT32* p = ((UINT32*)g_pVideoAddress) - 14;	// Point back to pixel-0
		// NB. For VT_COLOR_MONITOR_NTSC, also check color-burst so that TEXT and MIXED(HGR+TEXT) render the TEXT at the same offset (GH#341)
		if (g_eVideoType == VT_MONO_TV || g_eVideoType == VT_COLOR_TV || (g_eVideoType == VT_COLOR_MONITOR_NTSC && GetColorBurst()))
		{
			p[2] = 0;
			p[g_kFrameBufferWidth+2] = 0;	// Next line (there are 2 lines per Apple II scanline)
		}
		else
		{
			p[0] = 0;
			p[g_kFrameBufferWidth+0] = 0;	// Next line (there are 2 lines per Apple II scanline)
		}
	}
}
#endif

//===========================================================================
void updateScreenDoubleHires40 (long cycles6502) // wsUpdateVideoHires0
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}
	
	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t  m     = pMain[0];
				uint16_t bits  = g_aPixelDoubleMaskHGR[m & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				updatePixels( bits );
				// NB. No zeroPixel0_14M(), since no color phase shift (or use of g_nLastColumnPixelNTSC)
			}
		}
		updateVideoScannerHorzEOL();
	}
}

//===========================================================================

void updateScreenDoubleHires80Simplified (long cycles6502 ) // wsUpdateVideoDblHires
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint16_t addr = getVideoScannerAddressHGR();
				uint8_t a = *MemGetAuxPtr(addr);
				uint8_t m = *MemGetMainPtr(addr);

				if (RGB_Is160Mode())
				{
					int width = UpdateDHiRes160Cell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress);
					g_pVideoAddress += width;
				}
				else if (RGB_Is560Mode() || (RGB_IsMixMode() && !((a | m) & 0x80)))
				{
					update7MonoPixels(a);
					update7MonoPixels(m);
				}
				else if (!RGB_IsMixMode() || (RGB_IsMixMode() && (a & m & 0x80)))
				{
					UpdateDHiResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress, true, true);
					g_pVideoAddress += 14;
				}
				else	// RGB_IsMixMode() && ((a ^ m) & 0x80)
				{
					if (a & 0x80)	// RGB color, then monochrome
					{
						UpdateDHiResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress, true ,false);
						g_pVideoAddress += 7;
						update7MonoPixels(m);
					}
					else			// monochrome, then RGB color
					{
						update7MonoPixels(a);
						UpdateDHiResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress, false, true);
						g_pVideoAddress += 7;
					}
				}
			}
		}
		updateVideoScannerHorzEOLSimple();
	}
}

void updateScreenDoubleHires80 (long cycles6502 ) // wsUpdateVideoDblHires
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t  *pMain = MemGetMainPtr(addr);
				uint8_t  *pAux  = MemGetAuxPtr (addr);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				uint16_t bits = ((m & 0x7f) << 7) | (a & 0x7f);
				bits = (bits << 1) | g_nLastColumnPixelNTSC;
				updatePixels( bits );
				g_nLastColumnPixelNTSC = (bits >> 14) & 1;

#if EXTEND_14M_VIDEO_BY_1_PIXEL
				zeroPixel0_14M();
#endif
			}
		}
#if EXTEND_14M_VIDEO_BY_1_PIXEL
		updateVideoScannerHorzEOL_14M();
#else
		updateVideoScannerHorzEOL();
#endif
	}
}

//===========================================================================
void updateScreenDoubleLores40 (long cycles6502) // wsUpdateVideo7MLores
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t  m     = pMain[0];
				uint16_t lo    = getLoResBits( m ); 
				uint16_t bits  = g_aPixelDoubleMaskHGR[(0xFF & lo >> ((1 - (g_nVideoClockHorz & 1)) * 2)) & 0x7F]; // Optimization: hgrbits
				updatePixels( bits );
				// NB. No zeroPixel0_14M(), since no color phase shift (or use of g_nLastColumnPixelNTSC)
			}
		}
		updateVideoScannerHorzEOL();
	}
}

//===========================================================================

static void updateScreenDoubleLores80Simplified (long cycles6502) // wsUpdateVideoDblLores
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint16_t addr = getVideoScannerAddressTXT();
				UpdateDLoResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress);
				g_pVideoAddress += 14;
			}
		}
		updateVideoScannerHorzEOLSimple();
	}
}

void updateScreenDoubleLores80 (long cycles6502) // wsUpdateVideoDblLores
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t *pAux  = MemGetAuxPtr (addr);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				uint16_t lo = getLoResBits( m );
				uint16_t hi = getLoResBits( a );

				uint16_t main = lo >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				uint16_t aux  = hi >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				uint16_t bits = (main << 7) | (aux & 0x7f);
				updatePixels( bits );
				g_nLastColumnPixelNTSC = (bits >> 14) & 1;

#if EXTEND_14M_VIDEO_BY_1_PIXEL
				zeroPixel0_14M();
#endif
			}
		}
#if EXTEND_14M_VIDEO_BY_1_PIXEL
		updateVideoScannerHorzEOL_14M();
#else
		updateVideoScannerHorzEOL();
#endif

	}
}

//===========================================================================
static void updateScreenSingleHires40Simplified (long cycles6502)
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint16_t addr = getVideoScannerAddressHGR();
				UpdateHiResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress);
				g_pVideoAddress += 14;
			}
		}
		updateVideoScannerHorzEOLSimple();
	}
}

void updateScreenSingleHires40 (long cycles6502)
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t  m     = pMain[0];
				uint16_t bits  = g_aPixelDoubleMaskHGR[m & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				if (m & 0x80)
					bits = (bits << 1) | g_nLastColumnPixelNTSC;
				updatePixels( bits );

				// For last hpos && bit6=1: (GH#555)
				// * if bit7=0 (no shift) then clear g_nLastColumnPixelNTSC to prevent a 3rd 14M (aka DHGR) pixel being drawn
				//   . even though this is off-screen, it still has an on-screen affect (making the green dot more white on the screen edge).
				// * if bit7=1 (half-dot shift) then also clear g_nLastColumnPixelNTSC
				//   . not sure if this is correct though
				if (g_nVideoClockHorz == (VIDEO_SCANNER_MAX_HORZ-1))
					g_nLastColumnPixelNTSC = 0;
			}
		}
		updateVideoScannerHorzEOL();
	}
}

//===========================================================================
static void updateScreenSingleLores40Simplified (long cycles6502)
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint16_t addr = getVideoScannerAddressTXT();
				UpdateLoResCell(g_nVideoClockHorz-VIDEO_SCANNER_HORZ_START, g_nVideoClockVert, addr, g_pVideoAddress);
				g_pVideoAddress += 14;
			}
		}
		updateVideoScannerHorzEOLSimple();
	}
}

void updateScreenSingleLores40 (long cycles6502)
{
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pFuncUpdateTextScreen( cycles6502 );
		return;
	}

	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t  m     = pMain[0];
				uint16_t lo    = getLoResBits( m ); 
				uint16_t bits  = lo >> ((1 - (g_nVideoClockHorz & 1)) * 2);
				updatePixels( bits );
			}
		}
		updateVideoScannerHorzEOL();
	}
}

//===========================================================================
void updateScreenText40 (long cycles6502)
{
	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
		{
			if (g_nColorBurstPixels > 0)
				g_nColorBurstPixels -= 1;
		}
		else if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t  m     = pMain[0];
				uint8_t  c     = getCharSetBits(m);
				uint16_t bits  = g_aPixelDoubleMaskHGR[c & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128

				if (0 == g_nVideoCharSet && 0x40 == (m & 0xC0)) // Flash only if mousetext not active
					bits ^= g_nTextFlashMask;

				updatePixels( bits );

			}
		}
		updateVideoScannerHorzEOL();
	}
}

//===========================================================================
void updateScreenText80 (long cycles6502)
{
	for (; cycles6502 > 0; --cycles6502)
	{
		uint16_t addr = getVideoScannerAddressTXT();

		if ((g_nVideoClockHorz < VIDEO_SCANNER_HORZ_COLORBURST_END) && (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_COLORBURST_BEG))
		{
			if (g_nColorBurstPixels > 0)
				g_nColorBurstPixels -= 1;
		}
		else if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(addr);
				uint8_t *pAux  = MemGetAuxPtr (addr);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				uint16_t main = getCharSetBits( m );
				uint16_t aux  = getCharSetBits( a );

				if ((0 == g_nVideoCharSet) && 0x40 == (m & 0xC0)) // Flash only if mousetext not active
					main ^= g_nTextFlashMask;

				if ((0 == g_nVideoCharSet) && 0x40 == (a & 0xC0)) // Flash only if mousetext not active
					aux ^= g_nTextFlashMask;

				uint16_t bits = (main << 7) | (aux & 0x7f);
				if (g_eVideoType != VT_COLOR_MONITOR_RGB)			// No extra 14M bit needed for VT_COLOR_MONITOR_RGB
					bits = (bits << 1) | g_nLastColumnPixelNTSC;	// GH#555: Align TEXT80 chars with DHGR
				updatePixels( bits );
				g_nLastColumnPixelNTSC = (bits >> 14) & 1;

#if EXTEND_14M_VIDEO_BY_1_PIXEL
				zeroPixel0_14M();
#endif
			}
		}
#if EXTEND_14M_VIDEO_BY_1_PIXEL
		updateVideoScannerHorzEOL_14M();
#else
		updateVideoScannerHorzEOL();
#endif

	}
}

// Functions (Public) _____________________________________________________________________________

//===========================================================================
uint32_t*NTSC_VideoGetChromaTable( bool bHueTypeMonochrome, bool bMonitorTypeColorTV )
{
	if( bHueTypeMonochrome )
	{
		g_nChromaSize = sizeof( g_aBnwColorTV );

		if( bMonitorTypeColorTV )
			return (uint32_t*) g_aBnwColorTV;
		else
			return (uint32_t*) g_aBnWMonitor;
	} else {
		g_nChromaSize = sizeof( g_aHueColorTV );

		if( bMonitorTypeColorTV )
			return (uint32_t*) g_aHueColorTV;
		else
#if ALT_TABLE
			g_nChromaSize = sizeof(T_NTSC);
			return (uint32_t*)T_NTSC;
#endif
			return (uint32_t*) g_aHueMonitor;
	}
}

//===========================================================================
void NTSC_VideoClockResync(const DWORD dwCyclesThisFrame)
{
	g_nVideoClockVert = (uint16_t) (dwCyclesThisFrame / VIDEO_SCANNER_MAX_HORZ) % VIDEO_SCANNER_MAX_VERT;
	g_nVideoClockHorz = (uint16_t) (dwCyclesThisFrame % VIDEO_SCANNER_MAX_HORZ);
}

//===========================================================================
uint16_t NTSC_VideoGetScannerAddress ( const ULONG uExecutedCycles )
{
	if (g_bFullSpeed)
	{
		// Ensure that NTSC video-scanner gets updated during full-speed, so video-dependent Apple II code doesn't hang
		NTSC_VideoClockResync( CpuGetCyclesThisVideoFrame(uExecutedCycles) );
	}

	const uint16_t currVideoClockVert = g_nVideoClockVert;
	const uint16_t currVideoClockHorz = g_nVideoClockHorz;

	// Required for ANSI STORY (end credits) vert scrolling mid-scanline mixed mode: DGR80, TEXT80, DGR80
	g_nVideoClockHorz -= 2;
	if ((SHORT)g_nVideoClockHorz < 0)
	{
		g_nVideoClockHorz += VIDEO_SCANNER_MAX_HORZ;
		g_nVideoClockVert -= 1;
		if ((SHORT)g_nVideoClockVert < 0)
			g_nVideoClockVert = VIDEO_SCANNER_MAX_VERT-1;
	}

	uint16_t addr;
	bool bHires = (g_uVideoMode & VF_HIRES) && !(g_uVideoMode & VF_TEXT); // SW_HIRES && !SW_TEXT
	if( bHires )
		addr = getVideoScannerAddressHGR();
	else
		addr = getVideoScannerAddressTXT();

	g_nVideoClockVert = currVideoClockVert;
	g_nVideoClockHorz = currVideoClockHorz;

	return addr;
}

//===========================================================================
void NTSC_SetVideoTextMode( int cols )
{
	if( cols == 40 )
		g_pFuncUpdateTextScreen = updateScreenText40;
	else
		g_pFuncUpdateTextScreen = updateScreenText80;
}

//===========================================================================
void NTSC_SetVideoMode( uint32_t uVideoModeFlags )
{
	g_nVideoMixed   = uVideoModeFlags & VF_MIXED;
	g_nVideoCharSet = VideoGetSWAltCharSet() ? 1 : 0;

	g_nTextPage  = 1;
	g_nHiresPage = 1;
	if (uVideoModeFlags & VF_PAGE2)
	{
		// Apple IIe, Technical Notes, #3: Double High-Resolution Graphics
		// 80STORE must be OFF to display page 2
		if (0 == (uVideoModeFlags & VF_80STORE))
		{
			g_nTextPage  = 2;
			g_nHiresPage = 2;
		}
	}

	if (uVideoModeFlags & VF_TEXT)
	{
		if (uVideoModeFlags & VF_80COL)
			g_pFuncUpdateGraphicsScreen = updateScreenText80;
		else
			g_pFuncUpdateGraphicsScreen = updateScreenText40;
	}
	else if (uVideoModeFlags & VF_HIRES)
	{
		if (uVideoModeFlags & VF_DHIRES)
		{
			if (uVideoModeFlags & VF_80COL)
			{
				if (g_eVideoType == VT_COLOR_MONITOR_RGB)
					g_pFuncUpdateGraphicsScreen = updateScreenDoubleHires80Simplified;
				else
					g_pFuncUpdateGraphicsScreen = updateScreenDoubleHires80;
			}
			else
			{
				g_pFuncUpdateGraphicsScreen = updateScreenDoubleHires40;
			}
		}
		else
		{
			if (g_eVideoType == VT_COLOR_MONITOR_RGB)
				g_pFuncUpdateGraphicsScreen = updateScreenSingleHires40Simplified;
			else
				g_pFuncUpdateGraphicsScreen = updateScreenSingleHires40;
		}
	}
	else
	{
		if (uVideoModeFlags & VF_DHIRES)
		{
			if (uVideoModeFlags & VF_80COL)
			{
				if (g_eVideoType == VT_COLOR_MONITOR_RGB)
					g_pFuncUpdateGraphicsScreen = updateScreenDoubleLores80Simplified;
				else
					g_pFuncUpdateGraphicsScreen = updateScreenDoubleLores80;
			}
			else
			{
				g_pFuncUpdateGraphicsScreen = updateScreenDoubleLores40;
			}
		}
		else
		{
			if (g_eVideoType == VT_COLOR_MONITOR_RGB)
				g_pFuncUpdateGraphicsScreen = updateScreenSingleLores40Simplified;
			else
				g_pFuncUpdateGraphicsScreen = updateScreenSingleLores40;
		}
	}
}

//===========================================================================
void NTSC_SetVideoStyle() // (int v, int s)
{
    int half = IsVideoStyle(VS_HALF_SCANLINES);
	uint8_t r, g, b;

	switch ( g_eVideoType )
	{
		case VT_COLOR_TV:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			updateMonochromeTables( r, g, b );
			if (half)
			{
				g_pFuncUpdateBnWPixel = updatePixelBnWColorTVSingleScanline;
				g_pFuncUpdateHuePixel = updatePixelHueColorTVSingleScanline;
			}
			else {
				g_pFuncUpdateBnWPixel = updatePixelBnWColorTVDoubleScanline;
				g_pFuncUpdateHuePixel = updatePixelHueColorTVDoubleScanline;
			}
			break;

		case VT_COLOR_MONITOR_NTSC:
		default:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			updateMonochromeTables( r, g, b );
			if (half)
			{
				g_pFuncUpdateBnWPixel = updatePixelBnWMonitorSingleScanline;
				g_pFuncUpdateHuePixel = updatePixelHueMonitorSingleScanline;
			}
			else {
				g_pFuncUpdateBnWPixel = updatePixelBnWMonitorDoubleScanline;
				g_pFuncUpdateHuePixel = updatePixelHueMonitorDoubleScanline;
			}
			break;

		case VT_MONO_TV:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			updateMonochromeTables( r, g, b ); // Custom Monochrome color
			if (half)
			{
				g_pFuncUpdateBnWPixel = g_pFuncUpdateHuePixel = updatePixelBnWColorTVSingleScanline;
			}
			else {
				g_pFuncUpdateBnWPixel = g_pFuncUpdateHuePixel = updatePixelBnWColorTVDoubleScanline;
			}
			break;

		case VT_MONO_AMBER:
			r = 0xFF;
			g = 0x80;
			b = 0x00;
			goto _mono;

		case VT_MONO_GREEN:
			r = 0x00;
			g = 0xC0;
			b = 0x00;
			goto _mono;

		case VT_COLOR_MONITOR_RGB:
		case VT_MONO_WHITE:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			goto _mono;

		case VT_MONO_CUSTOM:
			// From WinGDI.h
			// #define RGB(r,g,b)         ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
			//#define GetRValue(rgb)      (LOBYTE(rgb))
			//#define GetGValue(rgb)      (LOBYTE(((WORD)(rgb)) >> 8))
			//#define GetBValue(rgb)      (LOBYTE((rgb)>>16))
			r = (g_nMonochromeRGB >>  0) & 0xFF;
			g = (g_nMonochromeRGB >>  8) & 0xFF;
			b = (g_nMonochromeRGB >> 16) & 0xFF;
_mono:
			updateMonochromeTables( r, g, b ); // Custom Monochrome color
			if (half)
			{
				g_pFuncUpdateBnWPixel = g_pFuncUpdateHuePixel = updatePixelBnWMonitorSingleScanline;
			}
			else
			{
				g_pFuncUpdateBnWPixel = g_pFuncUpdateHuePixel = updatePixelBnWMonitorDoubleScanline;
			}
			break;
		}
}

//===========================================================================
void GenerateVideoTables( void );
void GenerateBaseColors(baseColors_t pBaseNtscColors);

void NTSC_VideoInit( uint8_t* pFramebuffer ) // wsVideoInit
{
	make_csbits();
	GenerateVideoTables();
	initPixelDoubleMasks();
	initChromaPhaseTables();
	updateMonochromeTables( 0xFF, 0xFF, 0xFF );

	for (int y = 0; y < (VIDEO_SCANNER_Y_DISPLAY*2); y++)
	{
		uint32_t offset = sizeof(bgra_t) * GetFrameBufferWidth() * ((GetFrameBufferHeight() - 1) - y - GetFrameBufferBorderHeight()) + (sizeof(bgra_t) * GetFrameBufferBorderWidth());
		g_pScanLines[y] = (bgra_t*) (g_pFramebufferbits + offset);
	}

	g_pVideoAddress = g_pScanLines[0];

	g_pFuncUpdateTextScreen     = updateScreenText40;
	g_pFuncUpdateGraphicsScreen = updateScreenText40;

	VideoReinitialize(); // Setup g_pFunc_ntsc*Pixel()

	bgra_t baseColors[kNumBaseColors];
	GenerateBaseColors(&baseColors);
	VideoInitializeOriginal(&baseColors);

#if HGR_TEST_PATTERN
// Init HGR to almost all-possible-combinations
// CALL-151
// C050 C053 C057
	unsigned char b = 0;
	unsigned char *main, *aux;
	uint16_t ad;

	for( unsigned page = 0; page < 2; page++ )
	{
//		for( unsigned w = 0; w < 2; w++ ) // 16 cols
		{
			for( unsigned z = 0; z < 2; z++ ) // 8 cols
			{
				b  = 0; // 4 columns * 64 rows
				for( unsigned x = 0; x < 4; x++ ) // 4 cols
				{
					for( unsigned y = 0; y < 64; y++ ) // 1 col
					{
						unsigned y2 = y*2;
						ad = 0x2000 + (y2&7)*0x400 + ((y2/8)&7)*0x80 + (y2/64)*0x28 + 2*x + 10*z; // + 20*w;
						ad += 0x2000*page;
						main = MemGetMainPtr(ad);
						aux  = MemGetAuxPtr (ad);
						main[0] = b; main[1] = z + page*0x80;
						aux [0] = z; aux [1] = 0;

						if( page == 1 )
						{
							// Columns = # of consecutive pixels
							// x = 0, 1, 2, 3
							// # = 3, 5, 7, 9
							// b = 3, 7, 15, 31
							//   = (4 << x) - 1
							main[0+z] = (0x80*(y/32) + (((4 << x) - 1) << (y/8))); // (3 | 3+x*2)
							main[1+z] = (0x80*(y/32) + (((4 << x) - 1) << (y/8))) >> 8;
						}

						y2 = y*2 + 1;
						ad = 0x2000 + (y2&7)*0x400 + ((y2/8)&7)*0x80 + (y2/64)*0x28 + 2*x + 10*z; // + 20*w;
						ad += 0x2000*page;
						main = MemGetMainPtr(ad);
						aux  = MemGetAuxPtr (ad);
						main[0] =   0; main[1] = z + page*0x80;
						aux [0] =   b; aux [1] = 0;

						b++;
					}
				}
			}
		}
	}
#endif

}

//===========================================================================
void NTSC_VideoReinitialize( DWORD cyclesThisFrame )
{
	_ASSERT(cyclesThisFrame < VIDEO_SCANNER_6502_CYCLES);
	if (cyclesThisFrame >= VIDEO_SCANNER_6502_CYCLES) cyclesThisFrame = 0;	// error
	g_nVideoClockVert = (uint16_t) (cyclesThisFrame / VIDEO_SCANNER_MAX_HORZ);
	g_nVideoClockHorz = cyclesThisFrame % VIDEO_SCANNER_MAX_HORZ;

	updateVideoScannerAddress();	// Pre-condition: g_nVideoClockVert
}

//===========================================================================
void NTSC_VideoInitAppleType ()
{
	int model = GetApple2Type();

	// anything other than low bit set means not II/II+ (TC: include Pravets machines too?)
	if (model & 0xFFFE)
		g_pHorzClockOffset = APPLE_IIE_HORZ_CLOCK_OFFSET;
	else
		g_pHorzClockOffset = APPLE_IIP_HORZ_CLOCK_OFFSET;

	set_csbits();
}

//===========================================================================
void NTSC_VideoInitChroma()
{
	initChromaPhaseTables();
}

//===========================================================================

// NB. NTSC video-scanner doesn't get updated during full-speed, so video-dependent Apple II code can hang
//bool NTSC_VideoIsVbl ()
//{
//	return (g_nVideoClockVert >= VIDEO_SCANNER_Y_DISPLAY) && (g_nVideoClockVert < VIDEO_SCANNER_MAX_VERT);
//}

//===========================================================================

// Pre: cyclesLeftToUpdate = [0...VIDEO_SCANNER_6502_CYCLES]
// .  2-14: After one emulated 6502/65C02 opcode (optionally with IRQ)
// . ~1000: After 1ms of Z80 emulation
// . 17030: From NTSC_VideoRedrawWholeScreen()
static void VideoUpdateCycles( int cyclesLeftToUpdate )
{
	const int cyclesToEndOfLine = VIDEO_SCANNER_MAX_HORZ - g_nVideoClockHorz;

	if (g_nVideoClockVert < VIDEO_SCANNER_Y_MIXED)
	{
		const int cyclesToLine160 = VIDEO_SCANNER_MAX_HORZ * (VIDEO_SCANNER_Y_MIXED - g_nVideoClockVert - 1) + cyclesToEndOfLine;
		int cycles = cyclesLeftToUpdate < cyclesToLine160 ? cyclesLeftToUpdate : cyclesToLine160;
		g_pFuncUpdateGraphicsScreen(cycles);						// lines [currV...159]
		cyclesLeftToUpdate -= cycles;

		const int cyclesFromLine160ToLine261 = VIDEO_SCANNER_6502_CYCLES - (VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_Y_MIXED);
		cycles = cyclesLeftToUpdate < cyclesFromLine160ToLine261 ? cyclesLeftToUpdate : cyclesFromLine160ToLine261;
		g_pFuncUpdateGraphicsScreen(cycles);						// lines [160..191..261]
		cyclesLeftToUpdate -= cycles;

		// Any remaining cyclesLeftToUpdate: lines [0...currV)
	}
	else
	{
		const int cyclesToLine262 = VIDEO_SCANNER_MAX_HORZ * (VIDEO_SCANNER_MAX_VERT - g_nVideoClockVert - 1) + cyclesToEndOfLine;
		int cycles = cyclesLeftToUpdate < cyclesToLine262 ? cyclesLeftToUpdate : cyclesToLine262;
		g_pFuncUpdateGraphicsScreen(cycles);						// lines [currV...261]
		cyclesLeftToUpdate -= cycles;

		const int cyclesFromLine0ToLine159 = VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_Y_MIXED;
		cycles = cyclesLeftToUpdate < cyclesFromLine0ToLine159 ? cyclesLeftToUpdate : cyclesFromLine0ToLine159;
		g_pFuncUpdateGraphicsScreen(cycles);					// lines [0..159]
		cyclesLeftToUpdate -= cycles;

		// Any remaining cyclesLeftToUpdate: lines [160...currV)
	}

	if (cyclesLeftToUpdate)
		g_pFuncUpdateGraphicsScreen(cyclesLeftToUpdate);
}

//===========================================================================
void NTSC_VideoUpdateCycles( long cycles6502 )
{
	_ASSERT(cycles6502 < VIDEO_SCANNER_6502_CYCLES);	// Use NTSC_VideoRedrawWholeScreen() instead

	VideoUpdateCycles(cycles6502);
}

//===========================================================================
void NTSC_VideoRedrawWholeScreen( void )
{
#ifdef _DEBUG
	const uint16_t currVideoClockVert = g_nVideoClockVert;
	const uint16_t currVideoClockHorz = g_nVideoClockHorz;
#endif

	// (GH#405) For full-speed: whole screen updates will occur periodically
	// . The V/H pos will have been recalc'ed, so won't be continuous from previous (whole screen) update
	// . So the redraw must start at H-pos=0 & with the usual reinit for the start of a new line
	const uint16_t horz = g_nVideoClockHorz;
	g_nVideoClockHorz = 0;
	updateVideoScannerAddress();

	VideoUpdateCycles(VIDEO_SCANNER_6502_CYCLES);

	VideoUpdateCycles(horz);	// Finally update to get to correct H-pos

#ifdef _DEBUG
	_ASSERT(currVideoClockVert == g_nVideoClockVert);
	_ASSERT(currVideoClockHorz == g_nVideoClockHorz);
#endif
}

//===========================================================================

static bool CheckVideoTables2( eApple2Type type, uint32_t mode )
{
	SetApple2Type(type);
	NTSC_VideoInitAppleType();

	g_uVideoMode = mode;

	g_dwCyclesThisFrame = 0;
	g_nVideoClockHorz = g_nVideoClockVert = 0;

	for (DWORD cycles=0; cycles<VIDEO_SCANNER_MAX_VERT*VIDEO_SCANNER_MAX_HORZ; cycles++)
	{
		WORD addr1 = VideoGetScannerAddress(cycles);
		WORD addr2 = g_uVideoMode & VF_TEXT ? getVideoScannerAddressTXT()
											: getVideoScannerAddressHGR();
		_ASSERT(addr1 == addr2);
		if (addr1 != addr2)
		{
			char str[80];
			sprintf(str, "vpos=%04X, hpos=%02X, Video_adr=$%04X, NTSC_adr=$%04X\n", g_nVideoClockVert, g_nVideoClockHorz, addr1, addr2);
			OutputDebugString(str);
			return false;
		}

		g_nVideoClockHorz++;
		if (g_nVideoClockHorz == VIDEO_SCANNER_MAX_HORZ)
		{
			g_nVideoClockHorz = 0;
			g_nVideoClockVert++;
		}
	}

	return true;
}

static void CheckVideoTables( void )
{
	CheckVideoTables2(A2TYPE_APPLE2PLUS, VF_HIRES);
	CheckVideoTables2(A2TYPE_APPLE2PLUS, VF_TEXT);
	CheckVideoTables2(A2TYPE_APPLE2E,    VF_HIRES);
	CheckVideoTables2(A2TYPE_APPLE2E,    VF_TEXT);
}

static void GenerateVideoTables( void )
{
	eApple2Type currentApple2Type = GetApple2Type();

	//
	// g_aClockVertOffsetsHGR[]
	//

	g_uVideoMode = VF_HIRES;
	for (UINT i=0, cycle=VIDEO_SCANNER_HORZ_START; i<VIDEO_SCANNER_MAX_VERT; i++, cycle+=VIDEO_SCANNER_MAX_HORZ)
	{
		g_aClockVertOffsetsHGR[i] = VideoGetScannerAddress(cycle, VS_PartialAddrV);
		_ASSERT(g_aClockVertOffsetsHGR[i] == g_kClockVertOffsetsHGR[i]);
	}

	//
	// g_aClockVertOffsetsTXT[]
	//

	g_uVideoMode = VF_TEXT;
	for (UINT i=0, cycle=VIDEO_SCANNER_HORZ_START; i<(256+8)/8; i++, cycle+=VIDEO_SCANNER_MAX_HORZ*8)
	{
		g_aClockVertOffsetsTXT[i] = VideoGetScannerAddress(cycle, VS_PartialAddrV);
		_ASSERT(g_aClockVertOffsetsTXT[i] == g_kClockVertOffsetsTXT[i]);
	}

	//
	// APPLE_IIP_HORZ_CLOCK_OFFSET[]
	//

	g_uVideoMode = VF_TEXT;
	SetApple2Type(A2TYPE_APPLE2PLUS);
	for (UINT j=0; j<5; j++)
	{
		for (UINT i=0, cycle=j*64*VIDEO_SCANNER_MAX_HORZ; i<VIDEO_SCANNER_MAX_HORZ; i++, cycle++)
		{
			APPLE_IIP_HORZ_CLOCK_OFFSET[j][i] = VideoGetScannerAddress(cycle, VS_PartialAddrH);
			_ASSERT(APPLE_IIP_HORZ_CLOCK_OFFSET[j][i] == kAPPLE_IIP_HORZ_CLOCK_OFFSET[j][i]);
		}
	}

	//
	// APPLE_IIE_HORZ_CLOCK_OFFSET[]
	//

	g_uVideoMode = VF_TEXT;
	SetApple2Type(A2TYPE_APPLE2E);
	for (UINT j=0; j<5; j++)
	{
		for (UINT i=0, cycle=j*64*VIDEO_SCANNER_MAX_HORZ; i<VIDEO_SCANNER_MAX_HORZ; i++, cycle++)
		{
			APPLE_IIE_HORZ_CLOCK_OFFSET[j][i] = VideoGetScannerAddress(cycle, VS_PartialAddrH);
			_ASSERT(APPLE_IIE_HORZ_CLOCK_OFFSET[j][i] == kAPPLE_IIE_HORZ_CLOCK_OFFSET[j][i]);
		}
	}

	//

	CheckVideoTables();

	VideoResetState();
	SetApple2Type(currentApple2Type);
}

void GenerateBaseColors(baseColors_t pBaseNtscColors)
{
	for (UINT i=0; i<16; i++)
	{
		g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
		g_nSignalBitsNTSC = 0;

		// 12 iterations for colour to "stabilise", then 4 iterations to calc the average
		// - after colour "stabilises" then it repeats through 4 phases (with different RGB values for each phase)
		uint32_t bits = (i<<12) | (i<<8) | (i<<4) | i;	// 16 bits

		uint32_t colors[4];
		for (UINT j=0; j<16; j++)
		{
			colors[j&3] = getScanlineColor(bits & 1, g_aHueColorTV[g_nColorPhaseNTSC]);
			bits >>= 1;
			updateColorPhase();
		}

		int r = (((colors[0]>>16)&0xff) + ((colors[1]>>16)&0xff) + ((colors[2]>>16)&0xff) + ((colors[3]>>16)&0xff)) / 4;
		int g = (((colors[0]>> 8)&0xff) + ((colors[1]>> 8)&0xff) + ((colors[2]>> 8)&0xff) + ((colors[3]>> 8)&0xff)) / 4;
		int b = (((colors[0]    )&0xff) + ((colors[1]    )&0xff) + ((colors[2]    )&0xff) + ((colors[3]    )&0xff)) / 4;
		uint32_t color = ((r<<16) | (g<<8) | b) | ALPHA32_MASK;

		(*pBaseNtscColors)[i] = * (bgra_t*) &color;
	}
}
