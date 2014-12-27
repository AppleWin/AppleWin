/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms

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
#include "StdAfx.h"
#include "cs.h"
#include "wsvideo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> // uint8_t

#ifndef CHROMA_BLUR
	#define CHROMA_BLUR      1 // Default: 1; 1 = blur along ~8 pixels; 0 = sharper
#endif

#ifndef CHROMA_FILTER
	#define CHROMA_FILTER    1 // If no chroma blur; 0 = use chroma as-is, 1 = soft chroma blur, strong color fringes 2 = more blur, muted chroma fringe
#endif

#define HGR_TEST_PATTERN 0

// from Frame.h (Must keep in sync!)
#define FRAMEBUFFER_W 600
#define FRAMEBUFFER_H 420

// prototype from CPU.h
unsigned char CpuRead(unsigned short addr, unsigned long uExecutedCycles);
// prototypes from Memory.h
unsigned char * MemGetAuxPtr (unsigned short);
unsigned char * MemGetMainPtr (unsigned short);

//
	void init_chroma_phase_table();

int wsVideoCharSet = 0;
int wsVideoMixed = 0;
int wsHiresPage = 1;
int wsTextPage = 1;

// Understanding the Apple II, Timing Generation and the Video Scanner, Pg 3-11
// Vertical Scanning
// Horizontal Scanning
// "There are exactly 17030 (65 x 262) 6502 cycles in every television scan of an American Apple."
#define VIDEO_SCANNER_MAX_HORZ  65
#define VIDEO_SCANNER_MAX_VERT 262
static unsigned g_nVideoClockVert = 0; // 9-bit: VC VB VA V5 V4 V3 V2 V1 V0 = 0 .. 262
static unsigned g_nVideoClockHorz = 0; // 6-bit:          H5 H4 H3 H2 H1 H0 = 0 .. 64, 25 >= visible

unsigned g_aHorzClockMemAddress[65];
unsigned char wsTouched[32768];
unsigned char * wsLines[384];

unsigned wsFlashidx = 0;
unsigned wsFlashmask = 0;

static unsigned grbits[16];
static uint16_t g_aPixelDoubleMaskHGR[128]; // hgrbits -> g_aPixelDoubleMaskHGR

static unsigned hgr_vcoffs[262] = {
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

static unsigned txt_vcoffs[33] = {
	0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
	0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
	0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,
	0x0000,0x0080,0x0100,0x0180,0x0200,0x0280,0x0300,0x0380,0x380
};

static unsigned ii_hcoffs[5][65] = {
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

static unsigned std_hcoffs[5][65] = {
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

static unsigned (*hcoffs)[65] = std_hcoffs;

#define TEXT_ADDRESS() (txt_vcoffs[g_nVideoClockVert/8] + hcoffs    [g_nVideoClockVert/64][g_nVideoClockHorz] + (wsTextPage  *  0x400))
#define HGR_ADDRESS()  (hgr_vcoffs[g_nVideoClockVert  ] + std_hcoffs[g_nVideoClockVert/64][g_nVideoClockHorz] + (wsHiresPage * 0x2000))

void wsVideoInitModel (int model)
{
	// anything other than low bit set means not II/II+
	if (model & 0xFFFE)
		hcoffs = std_hcoffs;
	else
		hcoffs = ii_hcoffs;
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
		grbits[ color ] = (color << 12) | (color << 8) | (color << 4) | (color << 0);
}

static unsigned char *vbp0;
static int sbitstab[192][66];
static int lastsignal;
static int colorBurst;

#define NTSC_NUM_PHASES     4
#define NTSC_NUM_SEQUENCES  4096
#define NTSC_PIXEL_WIDTH    4
static unsigned char NTSCMono                    [NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCColor  [NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCMonoTV                  [NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCColorTV[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];

#define SIGZEROS 2
#define SIGPOLES 2
#define SIGGAIN  7.614490548e+00

static double signal_prefilter (double z)
{
	static double xv[SIGZEROS+1] = { 0,0,0 };
	static double yv[SIGPOLES+1] = { 0,0,0 };

	xv[0] = xv[1];
	xv[1] = xv[2]; 
	xv[2] = z / SIGGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2]; 
	yv[2] = xv[0] + xv[2] + (2.0 * xv[1]) + (-0.2718798058 * yv[0]) + (0.7465656072 * yv[1]);

	return yv[2];
}

#define LUMZEROS 2
#define LUMPOLES 2
//#define LUMGAIN  1.062635655e+01
//#define LUMCOEF1  -0.3412038399
//#define LUMCOEF2  0.9647813115
#define LUMGAIN  1.371331570e+01
#define LUMCOEF1 -0.3961075449
#define LUMCOEF2 1.1044202472

static double luma0_filter (double z)
{
	static double xv[LUMZEROS+1];
	static double yv[LUMPOLES+1];

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / LUMGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2]; 
	yv[2] = xv[0] + xv[2] + (2 * xv[1]) + (LUMCOEF1 * yv[0]) + (LUMCOEF2 * yv[1]);

	return yv[2];
}

static double luma1_filter (double z)
{
	static double xv[LUMZEROS+1];
	static double yv[LUMPOLES+1];

	xv[0] = xv[1];
	xv[1] = xv[2];
	xv[2] = z / LUMGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2]; 
	yv[2] = xv[0] + xv[2] + (2 * xv[1]) + (LUMCOEF1 * yv[0]) + (LUMCOEF2 * yv[1]);

	return yv[2];
}

#define CHRZEROS 2
#define CHRPOLES 2
#define CHRGAIN  7.438011255e+00

static double chroma_filter (double z)
{
	static double xv[CHRZEROS+1];
	static double yv[CHRPOLES+1];

	xv[0] = xv[1];
	xv[1] = xv[2]; 
	xv[2] = z / CHRGAIN;
	yv[0] = yv[1];
	yv[1] = yv[2]; 
	yv[2] = xv[2] - xv[0] + (-0.7318893645 * yv[0]) + (1.2336442711 * yv[1]);

	return yv[2];
}

#define PI 3.1415926535898
#define DEG_TO_RAD(x) (PI*(x)/180.) // 2PI=360, PI=180

#if CHROMA_BLUR
	#define CYCLESTART (PI/4.0) // PI/4 = 45 degrees
#else // sharpness is higher, less color bleed
	#if CHROMA_FILTER == 2
		#define CYCLESTART (PI/4.0) // PI/4 = 45 degrees // c = signal_prefilter(z);
	#else
//		#define CYCLESTART DEG_TO_RAD(90) // (PI*0.5) // PI/2 = 90 degrees // HGR: Great, GR: fail on brown
		#define CYCLESTART DEG_TO_RAD(115) // GR perfect match of slow method
	#endif
#endif

// Build the 4 phase chroma lookup table
// The YI'Q' colors are hard-coded
static void init_chroma_phase_table (void)
{
	int p,s,t,n;
	double z,y0,y1,c,i,q,r,g,b;
	double phi,zz;
	
	for (p = 0; p < 4; ++p)
	{
		phi = p * PI / 2.0 + CYCLESTART;
		for (s = 0; s < NTSC_NUM_SEQUENCES; ++s)
		{
			t = s;
			y0 = y1 = c = i = q = 0.0;

			for (n = 0; n < 12; ++n)
			{
				z = (double)(0 != (t & 0x800));
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
					c = c * 2.0;
					i = i + (c * cos(phi) - i) / 8.0;
					q = q + (c * sin(phi) - q) / 8.0;

					phi += (PI / 4);
					if (fabs((2 * PI) - phi) < 0.001) phi = phi - 2 * PI;
				} // k
			} // samples

			     if (z < 0.0) NTSCMono[s][0] = NTSCMono[s][1] = NTSCMono[s][2] = 0;
			else if (z > 1.0) NTSCMono[s][0] = NTSCMono[s][1] = NTSCMono[s][2] = 255;
			             else NTSCMono[s][0] = NTSCMono[s][1] = NTSCMono[s][2] = (unsigned char)(z * 255);

			     if (y1 < 0.0) NTSCMonoTV[s][0] = NTSCMonoTV[s][1] = NTSCMonoTV[s][2] = 0;
			else if (y1 > 1.0) NTSCMonoTV[s][0] = NTSCMonoTV[s][1] = NTSCMonoTV[s][2] = 255;
			              else NTSCMonoTV[s][0] = NTSCMonoTV[s][1] = NTSCMonoTV[s][2] = (unsigned char)(y1 * 255);
			
			NTSCMono[s][3] = NTSCMonoTV[s][3] = 255;
			
			r = y0 + 0.956 * i + 0.621 * q;
			g = y0 - 0.272 * i - 0.647 * q;
			b = y0 - 1.105 * i + 1.702 * q;
			
			     if (b < 0.0) NTSCColor[p][s][0] = 0;
			else if (b > 1.0) NTSCColor[p][s][0] = 255;
			             else NTSCColor[p][s][0] = (unsigned char)(b * 255);
			
			     if (g < 0.0) NTSCColor[p][s][1] = 0;
			else if (g > 1.0) NTSCColor[p][s][1] = 255;
			             else NTSCColor[p][s][1] = (unsigned char)(g * 255);
			
			     if (r < 0.0) NTSCColor[p][s][2] = 0;
			else if (r > 1.0) NTSCColor[p][s][2] = 255;
			             else NTSCColor[p][s][2] = (unsigned char)(r * 255);
			
			r = y1 + 0.956 * i + 0.621 * q;
			g = y1 - 0.272 * i - 0.647 * q;
			b = y1 - 1.105 * i + 1.702 * q;
			
			     if (b < 0.0) NTSCColorTV[p][s][0] = 0;
			else if (b > 1.0) NTSCColorTV[p][s][0] = 255;
			             else NTSCColorTV[p][s][0] = (unsigned char)(b * 255);
			
			     if (g < 0.0) NTSCColorTV[p][s][1] = 0;
			else if (g > 1.0) NTSCColorTV[p][s][1] = 255;
			             else NTSCColorTV[p][s][1] = (unsigned char)(g * 255);
			
			     if (r < 0.0) NTSCColorTV[p][s][2] = 0;
			else if (r > 1.0) NTSCColorTV[p][s][2] = 255;
			             else NTSCColorTV[p][s][2] = (unsigned char)(r * 255);
			
			NTSCColor[p][s][3] = NTSCColorTV[p][s][3] = 255;
		}
	}
}

void wsVideoInit ()
{
	int i,j;

	for (i = 0; i < 32768; ++i)
		wsTouched[i] = 8;
	
	for (j = 0; j < 192; ++j)
		for (i = 0; i < 66; ++i)
			sbitstab[j][i] = 0;
	
	make_csbits();
	init_video_tables();

	init_chroma_phase_table();

	vbp0 = wsLines[0];

#if HGR_TEST_PATTERN
// Michael -- Init HGR to almsot all-possible-combinations
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

#define INITIAL_COLOR_PHASE 0
static int colorPhase = INITIAL_COLOR_PHASE;
static int sbits = 0;

#define SINGLEPIXEL(signal,table) \
	do { \
		unsigned int *cp, *mp; \
		sbits = ((sbits << 1) | signal) & 0xFFF; \
		cp = (unsigned int *)(&(table[sbits][0])); \
		*((unsigned int *)vbp0) = *cp; \
		mp = (unsigned int *)(vbp0 - 4 * FRAMEBUFFER_W); \
		*mp = ((*cp & 0x00fcfcfc) >> 2) + 0xff000000; \
		vbp0 += 4; \
	} while(0)

#define SINGLETVPIXEL(signal,table) \
	do { \
		unsigned int ntscp, prevp, betwp; \
		unsigned int *prevlin, *between; \
		sbits = ((sbits << 1) | signal) & 0xFFF; \
		prevlin = (unsigned int *)(vbp0 + 8 * FRAMEBUFFER_W); \
		between = (unsigned int *)(vbp0 + 4 * FRAMEBUFFER_W); \
		ntscp = *(unsigned int *)(&(table[sbits][0])); /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ntscp - ((ntscp & 0x00fcfcfc) >> 2); \
		*between = betwp | 0xff000000; \
		*((unsigned int *)vbp0) = ntscp; \
		vbp0 += 4; \
	} while(0)

#define DOUBLEPIXEL(signal,table) \
	do { \
		unsigned int *cp, *mp; \
		sbits = ((sbits << 1) | signal) & 0xFFF; \
		cp = (unsigned int *)(&(table[sbits][0])); \
		mp = (unsigned int *)(vbp0 - 4 * FRAMEBUFFER_W); \
		*((unsigned int *)vbp0) = *mp = *cp; \
		vbp0 += 4; \
	} while(0)

#define DOUBLETVPIXEL(signal,table) \
	do { \
		unsigned int ntscp, prevp, betwp; \
		unsigned int *prevlin, *between; \
		sbits = ((sbits << 1) | signal) & 0xFFF; \
		prevlin = (unsigned int *)(vbp0 + 8 * FRAMEBUFFER_W); \
		between = (unsigned int *)(vbp0 + 4 * FRAMEBUFFER_W); \
		ntscp = *(unsigned int *)(&(table[sbits][0])); /* raw current NTSC color */ \
		prevp = *prevlin; \
		betwp = ((ntscp & 0x00fefefe) >> 1) + ((prevp & 0x00fefefe) >> 1); \
		*between = betwp | 0xff000000; \
		*((unsigned int *)vbp0) = ntscp; \
		vbp0 += 4; \
	} while(0)

static void ntscMonoSinglePixel (int compositeSignal)
{
	SINGLEPIXEL(compositeSignal, NTSCMono);
}

static void ntscMonoDoublePixel (int compositeSignal)
{
	DOUBLEPIXEL(compositeSignal, NTSCMono);
}

static void ntscColorSinglePixel (int compositeSignal)
{
	static int nextPhase[] = {1,2,3,0};
	SINGLEPIXEL(compositeSignal, NTSCColor[colorPhase]);
	colorPhase = nextPhase[colorPhase];
}

static void ntscColorDoublePixel (int compositeSignal)
{
	static int nextPhase[] = {1,2,3,0};
	DOUBLEPIXEL(compositeSignal, NTSCColor[colorPhase]);
	colorPhase = nextPhase[colorPhase];
}

static void ntscMonoTVSinglePixel (int compositeSignal)
{
	SINGLETVPIXEL(compositeSignal, NTSCMonoTV);
}

static void ntscMonoTVDoublePixel (int compositeSignal)
{
	DOUBLETVPIXEL(compositeSignal, NTSCMonoTV);
}

static void ntscColorTVSinglePixel (int compositeSignal)
{
	static int nextPhase[] = {1,2,3,0};
	SINGLETVPIXEL(compositeSignal, NTSCColorTV[colorPhase]);
	colorPhase = nextPhase[colorPhase];
}

static void ntscColorTVDoublePixel (int compositeSignal)
{
	static int nextPhase[] = {1,2,3,0};
	DOUBLETVPIXEL(compositeSignal, NTSCColorTV[colorPhase]);
	colorPhase = nextPhase[colorPhase];
}

static void (*ntscMonoPixel)(int) = ntscMonoSinglePixel;
static void (*ntscColorPixel)(int) = ntscColorSinglePixel;

void wsVideoStyle (int v, int s)
{
	switch (v)
	{
	case 0:
		if (s) {
			ntscMonoPixel = ntscMonoTVSinglePixel;
			ntscColorPixel = ntscColorTVSinglePixel;
		}
		else {
			ntscMonoPixel = ntscMonoTVDoublePixel;
			ntscColorPixel = ntscColorTVDoublePixel;
		}
		break;
	case 1:
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
	case 2:
		if (s) {
			ntscMonoPixel = ntscColorPixel = ntscMonoTVSinglePixel;
		}
		else {
			ntscMonoPixel = ntscColorPixel = ntscMonoTVDoublePixel;
		}
		break;
	case 3:
		if (s) {
			ntscMonoPixel = ntscColorPixel = ntscMonoSinglePixel;
		}
		else {
			ntscMonoPixel = ntscColorPixel = ntscMonoDoublePixel;
		}
		break;
	}
}

int wsVideoIsVbl ()
{
	return g_nVideoClockVert >= 192 && g_nVideoClockVert < 262;
}

unsigned char wsVideoByte (unsigned long cycle)
{
	unsigned char * mem;
	mem = MemGetMainPtr(g_aHorzClockMemAddress[ g_nVideoClockHorz ]);
	return mem[0];
}

#define VIDEO_DRAW_BITS() do { \
	if (colorBurst < 2) \
	{ \
		/* #1 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #2 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #3 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #4 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #5 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #6 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		/* #7 of 7 */ \
		ntscMonoPixel(bt & 1); bt >>= 1; \
		ntscMonoPixel(bt & 1); lastsignal = bt & 1; bt >>= 1;\
	} \
	else \
	{ \
		/* #1 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #2 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #3 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #4 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #5 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #6 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); bt >>= 1; \
		/* #7 of 7 */ \
		ntscColorPixel(bt & 1); bt >>= 1; \
		ntscColorPixel(bt & 1); lastsignal = bt & 1; bt >>= 1;\
	} \
} while(0)

#define VIDEO_DRAW_ENDLINE() do { \
	if (colorBurst < 2) \
	{ \
		ntscMonoPixel(lastsignal); \
		ntscMonoPixel(0); \
		ntscMonoPixel(0); \
		ntscMonoPixel(0); \
	} \
	else \
	{ \
		ntscColorPixel(lastsignal); \
		ntscColorPixel(0); \
		ntscColorPixel(0); \
		ntscColorPixel(0); \
	} \
} while(0)

#define END_OF_LINE() do { \
	g_nVideoClockHorz = 0; \
	if (g_nVideoClockVert < 192) VIDEO_DRAW_ENDLINE(); \
	if (++g_nVideoClockVert == 262) \
	{ \
		g_nVideoClockVert = 0; \
		if (++wsFlashidx == 16) \
		{ \
			wsFlashidx = 0; \
			wsFlashmask ^= 0xffff; \
		} \
	} \
	if (g_nVideoClockVert < 192) \
	{ \
		vbp0 = wsLines[2*g_nVideoClockVert]; \
		colorPhase = INITIAL_COLOR_PHASE; \
		lastsignal = 0; \
		sbits = 0; \
	} \
} while(0)

void wsUpdateVideoText (long ticks)
{
	unsigned ad, bt;

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
//		updateVideoAddressHorzClock(); 
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
		{
			if (colorBurst > 0)
				colorBurst -= 1;
		}
		else if (g_nVideoClockVert < 192)
		{
			if (g_nVideoClockHorz >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);

				bt = g_aPixelDoubleMaskHGR[(csbits[wsVideoCharSet][main[0]][g_nVideoClockVert & 7]) & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				if (0 == wsVideoCharSet && 0x40 == (main[0] & 0xC0))
					bt ^= wsFlashmask;
				VIDEO_DRAW_BITS();
			}
		}
			
		/* increment scanner */
		if (++g_nVideoClockHorz == 65)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblText (long ticks)
{
	unsigned int ad, bt, mbt, abt;

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
//		updateVideoAddressHorzClock(); 
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
		{
			if (colorBurst > 0)
				colorBurst -= 1;
		}
		else if (g_nVideoClockVert < 192)
		{
			if (g_nVideoClockHorz >= 25)
			{
				unsigned char * aux = MemGetAuxPtr(ad);
				unsigned char * main = MemGetMainPtr(ad);
				
				mbt = csbits[wsVideoCharSet][main[0]][g_nVideoClockVert & 7];
				if (0 == wsVideoCharSet && 0x40 == (main[0] & 0xC0)) mbt ^= wsFlashmask;

				abt = csbits[wsVideoCharSet][aux[0]][g_nVideoClockVert & 7];
				if (0 == wsVideoCharSet && 0x40 == (aux[0] & 0xC0)) abt ^= wsFlashmask;

				bt = (mbt << 7) | abt;
				VIDEO_DRAW_BITS();
			}
		}
			
		/* increment scanner */
//		checkHorzClockEnd();
		if (++g_nVideoClockHorz == 65)
			END_OF_LINE();
	}
}

void wsUpdateVideo7MLores (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if (g_nVideoClockHorz < 16 && g_nVideoClockHorz >= 12)
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = g_aPixelDoubleMaskHGR[(0xFF & grbits[(main[0] >> (g_nVideoClockVert & 4)) & 0xF] >> ((1 - (g_nVideoClockHorz & 1)) * 2)) & 0x7F]; // Optimization: hgrbits
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void wsUpdateVideoLores (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = grbits[(main[0] >> (g_nVideoClockVert & 4)) & 0xF] >> ((1 - (g_nVideoClockHorz & 1)) * 2);
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblLores (long ticks)
{
	unsigned ad, bt, abt, mbt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * aux = MemGetAuxPtr(ad);
				unsigned char * main = MemGetMainPtr(ad);

				abt = grbits[(aux [0] >> (g_nVideoClockVert & 4)) & 0xF] >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				mbt = grbits[(main[0] >> (g_nVideoClockVert & 4)) & 0xF] >> (((1 - (g_nVideoClockHorz & 1)) * 2) + 3);
				bt = (mbt << 7) | (abt & 0x7f);
			
				VIDEO_DRAW_BITS();
				lastsignal = bt & 1;
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblHires (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * aux = MemGetAuxPtr(ad);
				unsigned char * main = MemGetMainPtr(ad);

				bt = ((main[0] & 0x7f) << 7) | (aux[0] & 0x7f);
				bt = (bt << 1) | lastsignal;
				VIDEO_DRAW_BITS();
				lastsignal = bt & 1;
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void wsUpdateVideoHires (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);

				bt = g_aPixelDoubleMaskHGR[main[0] & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				if (main[0] & 0x80) bt = (bt << 1) | lastsignal;
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void wsUpdateVideoHires0 (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && g_nVideoClockVert >= 160)
	{
		wsVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
//		updateVideoAddressHorzClock();
		g_aHorzClockMemAddress[ g_nVideoClockHorz ] = ad;

		if (g_nVideoClockVert < 192)
		{
			if ((g_nVideoClockHorz < 16) && (g_nVideoClockHorz >= 12))
			{
				colorBurst = 1024;
			}
			else if (g_nVideoClockHorz >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = g_aPixelDoubleMaskHGR[main[0] & 0x7F]; // Optimization: hgrbits second 128 entries are mirror of first 128
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
//		checkHorzClockEnd();
		if (65 == ++g_nVideoClockHorz)
			END_OF_LINE();
	}
}

void (* wsVideoText)(long) = wsUpdateVideoText;
void (* wsVideoUpdate)(long) = wsUpdateVideoText;
