/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms
Copyright (C) 2014 Michael Pohoreski

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
	#include "AppleWin.h"
	#include "CPU.h"
	#include "Frame.h"  // FRAMEBUFFER_W FRAMEBUFFER_H
	#include "Memory.h" // MemGetMainPtr() MemGetBankPtr()
	#include "Video.h"  // g_pFramebufferbits

	#include "NTSC.h"
	#include "NTSC_CharSet.cpp"

	//LPBYTE  MemGetMainPtr(const WORD);
	//LPBYTE  MemGetBankPtr(const UINT nBank);

// Defines
	#define PI 3.1415926535898f
	#define RAD_45  PI*0.25f
	#define RAD_90  PI*0.5f
	#define RAD_360 PI*2.f

	#define DEG_TO_RAD(x) (PI*(x)/180.f) // 2PI=360, PI=180,PI/2=90,PI/4=45

	#ifndef CHROMA_BLUR
		#define CHROMA_BLUR      1 // Default: 1; 1 = blur along ~8 pixels; 0 = sharper
	#endif

	#ifndef CHROMA_FILTER
		#define CHROMA_FILTER    1 // If no chroma blur; 0 = use chroma as-is, 1 = soft chroma blur, strong color fringes 2 = more blur, muted chroma fringe
	#endif

	#if CHROMA_BLUR
		#define CYCLESTART (PI/4.f) // PI/4 = 45 degrees
	#else // sharpness is higher, less color bleed
		#if CHROMA_FILTER == 2
			#define CYCLESTART (PI/4.f) // PI/4 = 45 degrees // c = signal_prefilter(z);
		#else
	//		#define CYCLESTART DEG_TO_RAD(90) // (PI*0.5) // PI/2 = 90 degrees // HGR: Great, GR: fail on brown
			#define CYCLESTART DEG_TO_RAD(115.f) // GR perfect match of slow method
		#endif
	#endif

	#define HGR_TEST_PATTERN 0

// Globals (Public) ___________________________________________________
	uint16_t g_nVideoClockVert = 0; // 9-bit: VC VB VA V5 V4 V3 V2 V1 V0 = 0 .. 262
	uint16_t g_nVideoClockHorz = 0; // 6-bit:          H5 H4 H3 H2 H1 H0 = 0 .. 64, 25 >= visible

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

	#define VIDEO_SCANNER_HORZ_START 25 // first displayable horz scanner index
	#define VIDEO_SCANNER_Y_MIXED   160 // num scanlins for mixed graphics + text
	#define VIDEO_SCANNER_Y_DISPLAY 192 // max displayable scanlines

	uint16_t g_aHorzClockMemAddress[VIDEO_SCANNER_MAX_HORZ];
	unsigned char * g_NTSC_Lines[384];  // To maintain the 280x192 aspect ratio for 560px width, we double every scan line -> 560x384

	static unsigned g_nTextFlashCounter = 0;
	static uint16_t g_nTextFlashMask    = 0;

	static unsigned g_aPixelMaskGR[16];
	static uint16_t g_aPixelDoubleMaskHGR[128]; // hgrbits -> g_aPixelDoubleMaskHGR: 7-bit mono 280 pixels to 560 pixel doubling

#define UpdateVideoAddressTXT() g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad = (g_aClockVertOffsetsTXT[g_nVideoClockVert/8] + g_pHorzClockOffset         [g_nVideoClockVert/64][g_nVideoClockHorz] + (g_nTextPage  *  0x400))
#define UpdateVideoAddressHGR() g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad = (g_aClockVertOffsetsHGR[g_nVideoClockVert  ] + APPLE_IIE_HORZ_CLOCK_OFFSET[g_nVideoClockVert/64][g_nVideoClockHorz] + (g_nHiresPage * 0x2000)) // BUG? g_pHorzClockOffset

	static unsigned char *vbp0;
	static int g_nLastColumnPixelNTSC;
	static int g_nColorBurstPixels;

	#define INITIAL_COLOR_PHASE 0
	static int g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
	static int g_nSignalBitsNTSC = 0;

	#define NTSC_NUM_PHASES     4
	#define NTSC_NUM_SEQUENCES  4096
	enum ColorChannel
	{	// Win32 DIB: BGRA format
		_B = 0,
		_G = 1,
		_R = 2,
		_A = 3,
		NUM_COLOR_CHANNELS = 4
	};

	static unsigned char g_aNTSCMonoMonitor                     [NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];
	static unsigned char g_aNTSCColorMonitor   [NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];
	static unsigned char g_aNTSCMonoTelevision                  [NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];
	static unsigned char g_aNTSCColorTelevision[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];

// g_aNTSCMonoMonitor    * g_nMonochromeRGB -> g_aNTSCMonoMonitorCustom
// g_aNTSCMonoTelevision * g_nMonochromeRGB -> g_aNTSCMonoTelevisionCustom
	static unsigned char g_aNTSCMonoMonitorCustom               [NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];
	static unsigned char g_aNTSCMonoTelevisionCustom            [NTSC_NUM_SEQUENCES][NUM_COLOR_CHANNELS];

	#define NUM_SIGZEROS 2
	#define NUM_SIGPOLES 2
	#define SIGGAIN  7.614490548f

	#define NUM_LUMZEROS 2
	#define NUM_LUMPOLES 2
	//#define LUMGAIN  1.062635655e+01
	//#define LUMCOEF1  -0.3412038399
	//#define LUMCOEF2  0.9647813115
	#define LUMGAIN  13.71331570f
	#define LUMCOEF1 -0.3961075449f
	#define LUMCOEF2 1.1044202472f

	#define NUM_CHRZEROS 2
	#define NUM_CHRPOLES 2
	#define CHRGAIN  7.438011255f

// Tables
	static unsigned g_aClockVertOffsetsHGR[ VIDEO_SCANNER_MAX_VERT ] = 
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

	static unsigned g_aClockVertOffsetsTXT[33] = 
	{
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
		0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,0x380
	};

	static unsigned APPLE_IIP_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ] =
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

	static unsigned APPLE_IIE_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ] =
	{
		{0x0068,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x106F,
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

	static unsigned (*g_pHorzClockOffset)[VIDEO_SCANNER_MAX_HORZ] = 0;

	static void (* g_pNTSC_FuncVideoText  )(long) = NTSC_UpdateVideoText40;
	       void (* g_pNTSC_FuncVideoUpdate)(long) = NTSC_UpdateVideoText40;

// Prototypes
	// prototype from CPU.h
	//unsigned char CpuRead(unsigned short addr, unsigned long uExecutedCycles);
	// prototypes from Memory.h
	//unsigned char * MemGetAuxPtr (unsigned short);
	//unsigned char * MemGetMainPtr (unsigned short);
	void init_chroma_phase_table();
	void updateColorPhase();
	void updateVideoHorzEOL();
	void updateMonochromeColor( uint16_t r, uint16_t g, uint16_t b );

//===========================================================================
inline float clampZeroOne( const float & x )
{
	if (x < 0.f) return 0.f;
	if (x > 1.f) return 1.f;
	/* ...... */ return x;
}

//===========================================================================
inline void updateColorPhase()
{
	g_nColorPhaseNTSC++;
	g_nColorPhaseNTSC &= 3;
}

//===========================================================================
void NTSC_VideoInitAppleType ()
{
	int model = g_Apple2Type;

	// anything other than low bit set means not II/II+
	if (model & 0xFFFE)
		g_pHorzClockOffset = APPLE_IIE_HORZ_CLOCK_OFFSET;
	else
		g_pHorzClockOffset = APPLE_IIP_HORZ_CLOCK_OFFSET;
}

static void init_video_tables (void)
{
	/*
		Convert 7-bit monochrome luminance to 14-bit double pixel luminance
		Chroma will be applied later based on the color phase in ntscColorDoublePixel( luminanceBit )
		0x001 -> 0x0003
		0x002 -> 0x000C
		0x004 -> 0x0030
		0x008 -> 0x00C0
		0x100 -> 0x4000
	*/
	for (uint8_t byte = 0; byte < 0x80; byte++ ) // Optimization: hgrbits second 128 entries are mirror of first 128
		for (uint8_t bits = 0; bits < 7; bits++ ) // high bit = half pixel shift; pre-optimization: bits < 8
			if (byte & (1 << bits)) // pow2 mask
				g_aPixelDoubleMaskHGR[byte] |= 3 << (bits*2);

	for ( uint16_t color = 0; color < 16; color++ )
		g_aPixelMaskGR[ color ] = (color << 12) | (color << 8) | (color << 4) | (color << 0);
}

// sadly float64 precision is needed
#define real double

static real signal_prefilter (real z)
{
	static real xv[NUM_SIGZEROS + 1] = { 0,0,0 };
	static real yv[NUM_SIGPOLES + 1] = { 0,0,0 };

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / SIGGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2];
	yv[2] = xv[0] + xv[2] + (2.f * xv[1]) + (-0.2718798058f * yv[0]) + (0.7465656072f * yv[1]);

	return yv[2];
}

static real luma0_filter (real z)
{
	static real xv[NUM_LUMZEROS + 1];
	static real yv[NUM_LUMPOLES + 1];

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / LUMGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2];
	yv[2] = xv[0] + xv[2] + (2.f * xv[1]) + (LUMCOEF1 * yv[0]) + (LUMCOEF2 * yv[1]);

	return yv[2];
}

static real luma1_filter (real z)
{
	static real xv[NUM_LUMZEROS + 1];
	static real yv[NUM_LUMPOLES + 1];

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / LUMGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2];
	yv[2] = xv[0] + xv[2] + (2 * xv[1]) + (LUMCOEF1 * yv[0]) + (LUMCOEF2 * yv[1]);

	return yv[2];
}

static real chroma_filter (real z)
{
	static real xv[NUM_CHRZEROS + 1];
	static real yv[NUM_CHRPOLES + 1];

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / CHRGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2];
	yv[2] = xv[2] - xv[0] + (-0.7318893645f * yv[0]) + (1.2336442711f * yv[1]);

	return yv[2];
}

// Build the 4 phase chroma lookup table
// The YI'Q' colors are hard-coded
//===========================================================================
static void init_chroma_phase_table (void)
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
#if CHROMA_BLUR
					//z = z * 1.25;
					zz = signal_prefilter(z);
					c = chroma_filter(zz); // "Mostly" correct _if_ CYCLESTART = PI/4 = 45 degrees
					y0 = luma0_filter(zz);
					y1 = luma1_filter(zz - c);
#else // CHROMA_BLUR
					y0 = y0 + (z - y0) / 4.0;
					y1 = y0; // fix TV mode

	#if CHROMA_FILTER == 0
					c = z; // sharper; "Mostly" correct _if_ CYCLESTART = 115 degrees
	#endif // CHROMA_FILTER
	#if CHROMA_FILTER == 1 // soft chroma blur, strong color fringes
					// NOTE: This has incorrect colors! Chroma is (115-45)=70 degrees out of phase! violet <-> orange, green <-> blue
					c = (z - y0); // Original -- smoother, white is solid, brighter; other colors
					//   ->
					// c = (z - (y0 + (z-y0)/4))
					// c = z - y0 - (z-y0)/4
					// c = z - y0 - z/4 + y0/4
					// c = z-z/4 - y0+y0/4; // Which is clearly wrong, unless CYCLESTART DEG_TO_RAD(115)
					// This mode looks the most accurate for white, has better color fringes
	#endif
	#if CHROMA_FILTER == 2 // more blur, muted chroma fringe
					// White has too much ringing, and the color fringes are muted
					c = signal_prefilter(z); // "Mostly" correct _if_ CYCLESTART = PI/4 = 45 degrees
	#endif
#endif // CHROMA_BLUR
					c = c * 2.f;
					i = i + (c * cos(phi) - i) / 8.f;
					q = q + (c * sin(phi) - q) / 8.f;

					phi += RAD_45; //(PI / 4);
					if (fabs((RAD_360) - phi) < 0.001)
						phi = phi - RAD_360; // 2 * PI;
				} // k
			} // samples

			brightness = clampZeroOne( (float)z );
			g_aNTSCMonoMonitor[s][_B] = (uint8_t)(brightness * 255);
			g_aNTSCMonoMonitor[s][_G] = (uint8_t)(brightness * 255);
			g_aNTSCMonoMonitor[s][_R] = (uint8_t)(brightness * 255);
			g_aNTSCMonoMonitor[s][_A] = 255;

			brightness = clampZeroOne( (float)y1);
			g_aNTSCMonoTelevision[s][_B] = (uint8_t)(brightness * 255);
			g_aNTSCMonoTelevision[s][_G] = (uint8_t)(brightness * 255);
			g_aNTSCMonoTelevision[s][_R] = (uint8_t)(brightness * 255);
			g_aNTSCMonoTelevision[s][_A] = 255;
			
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

			g_aNTSCColorMonitor[phase][s][_B] = (uint8_t)(b32 * 255);
			g_aNTSCColorMonitor[phase][s][_G] = (uint8_t)(g32 * 255);
			g_aNTSCColorMonitor[phase][s][_R] = (uint8_t)(r32 * 255);
			g_aNTSCColorMonitor[phase][s][_A] = 255;
			
			r64 = y1 + (I_TO_R * i) + (Q_TO_R * q);
			g64 = y1 + (I_TO_G * i) + (Q_TO_G * q);
			b64 = y1 + (I_TO_B * i) + (Q_TO_B * q);
			
			b32 = clampZeroOne( (float)b64 );
			g32 = clampZeroOne( (float)g64 );
			r32 = clampZeroOne( (float)r64 );

			g_aNTSCColorTelevision[phase][s][_B] = (uint8_t)(b32 * 255);
			g_aNTSCColorTelevision[phase][s][_G] = (uint8_t)(g32 * 255);
			g_aNTSCColorTelevision[phase][s][_R] = (uint8_t)(r32 * 255);
			g_aNTSCColorTelevision[phase][s][_A] = 255;
		}
	}
}

//===========================================================================
void NTSC_VideoInit( uint8_t* pFramebuffer ) // wsVideoInit
{
	make_csbits();
	init_video_tables();
	init_chroma_phase_table();
	updateMonochromeColor( 0xFF, 0xFF, 0xFF );

	for (int y = 0; y < 384; y++)
		g_NTSC_Lines[y] = g_pFramebufferbits + 4 * FRAMEBUFFER_W * ((FRAMEBUFFER_H - 1) - y - 18) + 80;

	vbp0 = g_NTSC_Lines[0]; // wsLines

#if HGR_TEST_PATTERN
// Michael -- Init HGR to almost all-possible-combinations
// CALL-151
// C050 C053 C057
	unsigned char b = 0;
	unsigned char *main, *aux;

	for( unsigned page = 0; page < 2; page++ )
	{
		for( unsigned w = 0; w < 2; w++ ) // 16 cols
		{
			for( unsigned z = 0; z < 2; z++ ) // 8 cols
			{
				b  = 0; // 4 columns * 64 rows
				for( unsigned x = 0; x < 4; x++ ) // 4 cols
				{
					for( unsigned y = 0; y < 64; y++ ) // 1 col
					{
						unsigned y2 = y*2;
						ad = 0x2000 + (y2&7)*0x400 + ((y2/8)&7)*0x80 + (y2/64)*0x28 + 2*x + 10*z + 20*w;
						ad += 0x2000*page;
						main = MemGetMainPtr(ad);
						aux  = MemGetAuxPtr (ad);
						main[0] = b; main[1] = w + page*0x80;
						aux [0] = z; aux [1] = 0;

						y2 = y*2 + 1;
						ad = 0x2000 + (y2&7)*0x400 + ((y2/8)&7)*0x80 + (y2/64)*0x28 + 2*x + 10*z + 20*w;
						ad += 0x2000*page;
						main = MemGetMainPtr(ad);
						aux  = MemGetAuxPtr (ad);
						main[0] =   0; main[1] = w + page*0x80;
						aux [0] =   b; aux [1] = 0;

						b++;
					}
				}
			}
		}
	}
#endif

}

#define SINGLEPIXEL(signal,table) \
	do { \
		unsigned int *cp, *mp; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		cp = (unsigned int *)(&(table[g_nSignalBitsNTSC][0])); \
		*((unsigned int *)vbp0) = *cp; \
		mp = (unsigned int *)(vbp0 - 4 * FRAMEBUFFER_W); \
		*mp = ((*cp & 0x00fcfcfc) >> 2) + 0xff000000; \
		vbp0 += 4; \
	} while(0)

#define SINGLETVPIXEL(signal,table) \
	do { \
		unsigned int ntscp, prevp, betwp; \
		unsigned int *prevlin, *between; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		prevlin = (unsigned int *)(vbp0 + 8 * FRAMEBUFFER_W); \
		between = (unsigned int *)(vbp0 + 4 * FRAMEBUFFER_W); \
		ntscp = *(unsigned int *)(&(table[g_nSignalBitsNTSC][0])); /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ntscp - ((ntscp & 0x00fcfcfc) >> 2); \
		*between = betwp | 0xff000000; \
		*((unsigned int *)vbp0) = ntscp; \
		vbp0 += 4; \
	} while(0)

#define DOUBLEPIXEL(signal,table) \
	do { \
		unsigned int *cp, *mp; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		cp = (unsigned int *)(&(table[g_nSignalBitsNTSC][0])); \
		mp = (unsigned int *)(vbp0 - 4 * FRAMEBUFFER_W); \
		*((unsigned int *)vbp0) = *mp = *cp; \
		vbp0 += 4; \
	} while(0)

#define DOUBLETVPIXEL(signal,table) \
	do { \
		unsigned int ntscp, prevp, betwp; \
		unsigned int *prevlin, *between; \
		g_nSignalBitsNTSC = ((g_nSignalBitsNTSC << 1) | signal) & 0xFFF; \
		prevlin = (unsigned int *)(vbp0 + 8 * FRAMEBUFFER_W); \
		between = (unsigned int *)(vbp0 + 4 * FRAMEBUFFER_W); \
		ntscp = *(unsigned int *)(&(table[g_nSignalBitsNTSC][0])); /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ((ntscp & 0x00fefefe) >> 1) + ((prevp & 0x00fefefe) >> 1); \
		*between = betwp | 0xff000000; \
		*((unsigned int *)vbp0) = ntscp; \
		vbp0 += 4; \
	} while(0)

static void ntscMonoSinglePixel (int compositeSignal)
{
	SINGLEPIXEL(compositeSignal, g_aNTSCMonoMonitorCustom);
}

static void ntscMonoDoublePixel (int compositeSignal)
{
	DOUBLEPIXEL(compositeSignal, g_aNTSCMonoMonitorCustom);
}

static void ntscColorSinglePixel (int compositeSignal)
{
	SINGLEPIXEL(compositeSignal, g_aNTSCColorMonitor[g_nColorPhaseNTSC]);
	updateColorPhase();
}

static void ntscColorDoublePixel (int compositeSignal)
{
	DOUBLEPIXEL(compositeSignal, g_aNTSCColorMonitor[g_nColorPhaseNTSC]);
	updateColorPhase();
}


static void ntscMonoTVSinglePixel (int compositeSignal)
{
	SINGLETVPIXEL(compositeSignal, g_aNTSCMonoTelevisionCustom);
}

static void ntscMonoTVDoublePixel (int compositeSignal)
{
	DOUBLETVPIXEL(compositeSignal, g_aNTSCMonoTelevisionCustom);
}

static void ntscColorTVSinglePixel (int compositeSignal)
{
	SINGLETVPIXEL(compositeSignal, g_aNTSCColorTelevision[g_nColorPhaseNTSC]);
	updateColorPhase();
}

static void ntscColorTVDoublePixel (int compositeSignal)
{
	DOUBLETVPIXEL(compositeSignal, g_aNTSCColorTelevision[g_nColorPhaseNTSC]);
	updateColorPhase();
}

static void (*ntscMonoPixel )(int) = ntscMonoSinglePixel ;
static void (*ntscColorPixel)(int) = ntscColorSinglePixel;

//===========================================================================
void updateMonochromeColor( uint16_t r, uint16_t g, uint16_t b )
{
	for( int iSample = 0; iSample < NTSC_NUM_SEQUENCES; iSample++ )
	{
		g_aNTSCMonoMonitorCustom[ iSample ][ _B ] = (g_aNTSCMonoMonitor[ iSample ][_B] * b) >> 8;
		g_aNTSCMonoMonitorCustom[ iSample ][ _G ] = (g_aNTSCMonoMonitor[ iSample ][_G] * g) >> 8;
		g_aNTSCMonoMonitorCustom[ iSample ][ _R ] = (g_aNTSCMonoMonitor[ iSample ][_R] * r) >> 8;
		g_aNTSCMonoMonitorCustom[ iSample ][ _A ] = 0xFF;

		g_aNTSCMonoTelevisionCustom[ iSample ][ _B ] = (g_aNTSCMonoTelevision[ iSample ][_B] * b) >> 8;
		g_aNTSCMonoTelevisionCustom[ iSample ][ _G ] = (g_aNTSCMonoTelevision[ iSample ][_G] * g) >> 8;
		g_aNTSCMonoTelevisionCustom[ iSample ][ _R ] = (g_aNTSCMonoTelevision[ iSample ][_R] * r) >> 8;
		g_aNTSCMonoTelevisionCustom[ iSample ][ _A ] = 0xFF;
	}
}


//===========================================================================
void NTSC_SetVideoStyle() // (int v, int s)
{
	int v = g_eVideoType;
    int s = g_uHalfScanLines;
	uint8_t r, g, b;

	switch (v)
	{
		case VT_COLOR_TVEMU: // VT_COLOR_TV: // 0:
			if (s) {
				ntscMonoPixel = ntscMonoTVSinglePixel;
				ntscColorPixel = ntscColorTVSinglePixel;
			}
			else {
				ntscMonoPixel = ntscMonoTVDoublePixel;
				ntscColorPixel = ntscColorTVDoublePixel;
			}
			break;

		case VT_COLOR_STANDARD: // VT_COLOR_MONITOR: //1:
		default:
			if (s) {
				ntscMonoPixel = ntscMonoSinglePixel;
				ntscColorPixel = ntscColorSinglePixel;
			}
			else {
				ntscMonoPixel = ntscMonoDoublePixel;
				ntscColorPixel = ntscColorDoublePixel;
			}
			break;

		case VT_COLOR_TEXT_OPTIMIZED: // VT_MONO_TV: //2:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			updateMonochromeColor( r, g, b ); // Custom Monochrome color
			if (s) {
				ntscMonoPixel = ntscColorPixel = ntscMonoTVSinglePixel;
			}
			else {
				ntscMonoPixel = ntscColorPixel = ntscMonoTVDoublePixel;
			}
			break;

//		case VT_MONO_WHITE: //VT_MONO_MONITOR: //3:
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

		case VT_MONO_WHITE:
			r = 0xFF;
			g = 0xFF;
			b = 0xFF;
			goto _mono;

		case VT_MONO_HALFPIXEL_REAL:
			// From WinGDI.h
			// #define RGB(r,g,b)         ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
			//#define GetRValue(rgb)      (LOBYTE(rgb))
			//#define GetGValue(rgb)      (LOBYTE(((WORD)(rgb)) >> 8))
			//#define GetBValue(rgb)      (LOBYTE((rgb)>>16))
			r = (g_nMonochromeRGB >>  0) & 0xFF;
			g = (g_nMonochromeRGB >>  8) & 0xFF;
			b = (g_nMonochromeRGB >> 16) & 0xFF;
_mono:
			updateMonochromeColor( r, g, b ); // Custom Monochrome color
			if (s) {
				ntscMonoPixel = ntscColorPixel = ntscMonoSinglePixel;
			}
			else {
				ntscMonoPixel = ntscColorPixel = ntscMonoDoublePixel;
			}
			break;
		}
}

int NTSC_VideoIsVbl ()
{
	return (g_nVideoClockVert >= VIDEO_SCANNER_Y_DISPLAY) && (g_nVideoClockVert < VIDEO_SCANNER_MAX_VERT);
}

unsigned char NTSC_VideoByte (unsigned long cycle)
{
	unsigned char * mem;
	mem = MemGetMainPtr(g_aHorzClockMemAddress[ g_nVideoClockHorz ]);
	return mem[0];
}

inline
void VIDEO_DRAW_BITS( uint16_t bt )
{
	if (g_nColorBurstPixels < 2)
	{ 
		/* #1 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #2 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #3 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #4 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #5 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #6 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); bt >>= 1;
		/* #7 of 7 */
		ntscMonoPixel(bt & 1); bt >>= 1;
		ntscMonoPixel(bt & 1); g_nLastColumnPixelNTSC = bt & 1; bt >>= 1;
	}
	else
	{
		/* #1 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #2 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #3 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #4 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #5 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #6 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); bt >>= 1;
		/* #7 of 7 */
		ntscColorPixel(bt & 1); bt >>= 1;
		ntscColorPixel(bt & 1); g_nLastColumnPixelNTSC = bt & 1; bt >>= 1;
	}
}

inline
void updateVideoHorzEOL()
{
	if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
	{
		g_nVideoClockHorz = 0;
		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			//VIDEO_DRAW_ENDLINE();
			if (g_nColorBurstPixels < 2)
			{
				ntscMonoPixel(g_nLastColumnPixelNTSC);
				ntscMonoPixel(0);
				ntscMonoPixel(0);
				ntscMonoPixel(0);
			}
			else
			{
				ntscColorPixel(g_nLastColumnPixelNTSC);
				ntscColorPixel(0);
				ntscColorPixel(0);
				ntscColorPixel(0);
			}
		}

		if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
		{
			g_nVideoClockVert = 0;
			if (++g_nTextFlashCounter == 16)
			{
				g_nTextFlashCounter = 0;
				g_nTextFlashMask ^= -1; // 16-bits
			}
		}

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			vbp0 = g_NTSC_Lines[2*g_nVideoClockVert];
			g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
			g_nLastColumnPixelNTSC = 0;
			g_nSignalBitsNTSC = 0;
		}
	}
}

// Light-weight Video Clock Update
//===========================================================================
void NTSC_VideoUpdateCycles( long cycles )
{
//	if( !g_bFullSpeed )
//			g_pNTSC_FuncVideoUpdate( uElapsedCycles );
//	else
	for( ; cycles > 0; cycles-- )
	{
		if (VIDEO_SCANNER_MAX_HORZ == ++g_nVideoClockHorz)
		{
			g_nVideoClockHorz = 0;
			if (++g_nVideoClockVert == VIDEO_SCANNER_MAX_VERT)
			{
				g_nVideoClockVert = 0;
				if (++g_nTextFlashCounter == 16)
				{
					g_nTextFlashCounter = 0;
					g_nTextFlashMask ^= -1; // 16-bits
				}

				// Force full refresh
				g_pNTSC_FuncVideoUpdate( VIDEO_SCANNER_6502_CYCLES );
			}

			if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
			{
				vbp0 = g_NTSC_Lines[2*g_nVideoClockVert];
				g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
				g_nLastColumnPixelNTSC = 0;
				g_nSignalBitsNTSC = 0;
			}
		}
	}
}

inline
uint8_t getCharSetBits(int iBank)
{
    return csbits[g_nVideoCharSet][iBank][g_nVideoClockVert & 7];
}

//===========================================================================
void NTSC_UpdateVideoText40 (long ticks)
{
	unsigned ad;

	for (; ticks; --ticks)
	{
		UpdateVideoAddressTXT();

		if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
		{
			if (g_nColorBurstPixels > 0)
				g_nColorBurstPixels -= 1;
		}
		else if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t  m     = pMain[0];
				uint8_t  c     = getCharSetBits(m);
				uint16_t bits  = g_aPixelDoubleMaskHGR[c & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				if (0 == g_nVideoCharSet && 0x40 == (m & 0xC0))
					bits ^= g_nTextFlashMask;
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_UpdateVideoText80 (long ticks)
{
	unsigned int ad;

	for (; ticks; --ticks)
	{
		UpdateVideoAddressTXT();

		if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
		{
			if (g_nColorBurstPixels > 0)
				g_nColorBurstPixels -= 1;
		}
		else if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pAux  = MemGetAuxPtr(ad);
				uint8_t *pMain = MemGetMainPtr(ad);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				uint16_t main = getCharSetBits( m );
				uint16_t aux  = getCharSetBits( a );

				if ((0 == g_nVideoCharSet) && 0x40 == (m & 0xC0))
					main ^= g_nTextFlashMask;

				if ((0 == g_nVideoCharSet) && 0x40 == (a & 0xC0))
					aux ^= g_nTextFlashMask;

				uint16_t bits = (main << 7) | aux;
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

inline
uint16_t getLoResBits( uint8_t iByte )
{
	return g_aPixelMaskGR[ (iByte >> (g_nVideoClockVert & 4)) & 0xF ]; 
}

//===========================================================================
void NTSC_UpdateVideoDblLores40 (long ticks) // wsUpdateVideo7MLores
{
	unsigned ad;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		UpdateVideoAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t  m     = pMain[0];
				uint16_t lo    = getLoResBits( m ); 
				uint16_t bits  = g_aPixelDoubleMaskHGR[(0xFF & lo >> ((1 - (g_nVideoClockHorz & 1)) * 2)) & 0x7F]; // Optimization: hgrbits
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_UpdateVideoLores40 (long ticks)
{
	unsigned ad;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		UpdateVideoAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t  m     = pMain[0];
				uint16_t lo    = getLoResBits( m ); 
				uint16_t bits  = lo >> ((1 - (g_nVideoClockHorz & 1)) * 2);
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_UpdateVideoDblLores80 (long ticks) // wsUpdateVideoDblLores
{
	unsigned ad;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		UpdateVideoAddressTXT();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t *pAux  = MemGetAuxPtr(ad);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				uint16_t lo = getLoResBits( m );
				uint16_t hi = getLoResBits( a );

				uint16_t main = lo >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				uint16_t aux  = hi >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				uint16_t bits = (main << 7) | (aux & 0x7f);
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_UpdateVideoDblHires80 (long ticks) // wsUpdateVideoDblHires
{
	unsigned ad;
	uint16_t bits;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		UpdateVideoAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t  *pMain = MemGetMainPtr(ad);
				uint8_t  *pAux  = MemGetAuxPtr(ad);

				uint8_t m = pMain[0];
				uint8_t a = pAux [0];

				bits = ((m & 0x7f) << 7) | (a & 0x7f);
				bits = (bits << 1) | g_nLastColumnPixelNTSC;
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}


//===========================================================================
void NTSC_UpdateVideoHires40 (long ticks)
{
	unsigned ad;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		UpdateVideoAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t  m     = pMain[0];
				uint16_t bits  = g_aPixelDoubleMaskHGR[m & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				if (m & 0x80)
					bits = (bits << 1) | g_nLastColumnPixelNTSC;
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_UpdateVideoDblHires40 (long ticks) // wsUpdateVideoHires0
{
	unsigned ad;
	
	if (g_nVideoMixed && g_nVideoClockVert >= VIDEO_SCANNER_Y_MIXED)
	{
		g_pNTSC_FuncVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		UpdateVideoAddressHGR();

		if (g_nVideoClockVert < VIDEO_SCANNER_Y_DISPLAY)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				g_nColorBurstPixels = 1024;
			}
			else if (g_nVideoClockHorz >= VIDEO_SCANNER_HORZ_START)
			{
				uint8_t *pMain = MemGetMainPtr(ad);
				uint8_t  m     = pMain[0];
				uint16_t bits  = g_aPixelDoubleMaskHGR[m & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				VIDEO_DRAW_BITS( bits );
			}
		}
		updateVideoHorzEOL();
	}
}

//===========================================================================
void NTSC_SetVideoTextMode( int cols )
{
    if( cols == 40 )
		g_pNTSC_FuncVideoText = NTSC_UpdateVideoText40;
	else
		g_pNTSC_FuncVideoText = NTSC_UpdateVideoText80;
}

//===========================================================================
void NTSC_SetVideoMode( int bVideoModeFlags )
{
	g_nVideoMixed   = bVideoModeFlags & VF_MIXED;
	g_nVideoCharSet = g_nAltCharSetOffset;

	g_nTextPage  = 1;
	g_nHiresPage = 1;
	if (bVideoModeFlags & VF_PAGE2) {
		if (0 == (bVideoModeFlags & VF_80STORE)) {
			g_nTextPage  = 2;
			g_nHiresPage = 2;
		}
	}

	if (bVideoModeFlags & VF_TEXT) {
		if (bVideoModeFlags & VF_80COL)
			g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoText80;
		else
			g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoText40;
	}
	else if (bVideoModeFlags & VF_HIRES) {
		if (bVideoModeFlags & VF_DHIRES)
			if (bVideoModeFlags & VF_80COL)
				g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoDblHires80;
			else
				g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoDblHires40;
		else
			g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoHires40;
	}
	else {
		if (bVideoModeFlags & VF_DHIRES)
			if (bVideoModeFlags & VF_80COL)
				g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoDblLores80;
			else
				g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoDblLores40;
		else
			g_pNTSC_FuncVideoUpdate = NTSC_UpdateVideoLores40;
	}
}
