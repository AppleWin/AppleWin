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

#include "cs.h"
#include "wsvideo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// from Frame.h (Must keep in sync!)
#define FRAMEBUFFER_W 600
#define FRAMEBUFFER_H 420

// prototype from CPU.h
unsigned char CpuRead(unsigned short addr, unsigned long uExecutedCycles);
// prototypes from Memory.h
unsigned char * MemGetAuxPtr (unsigned short);
unsigned char * MemGetMainPtr (unsigned short);

int wsVideoCharSet = 0;
int wsVideoMixed = 0;
int wsHiresPage = 1;
int wsTextPage = 1;

struct wsVideoDirtyRect wsVideoNewDirtyRect;
#define dirty wsVideoNewDirtyRect

unsigned wsVideoAddress[65];
unsigned char wsTouched[32768];
unsigned char * wsLines[384];

unsigned wsFlashidx = 0;
unsigned wsFlashmask = 0;

static unsigned grbits[16];
static unsigned hgrbits[256];

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

#define TEXT_ADDRESS() (txt_vcoffs[vc/8] + hcoffs[vc/64][hc] + (wsTextPage * 0x400))
#define HGR_ADDRESS() (hgr_vcoffs[vc] + std_hcoffs[vc/64][hc] + (wsHiresPage * 0x2000))

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
	unsigned i, b, m;
	
	for (i = 0; i < 256; ++i) {
		b = 0;
		for (m = 0x40; m; m >>= 1) {
			b <<= 2;
			if (i & m) b |= 3;
		}
		hgrbits[i] = b;
	}
	
	for (i = 0; i < 16; ++i)	{
		b = i;
		b = b * 16 + b;
		b = b * 256 + b;
		grbits[i] = b;
	}
}

static unsigned char *vbp0;
static int sbitstab[192][66];
static int lastsignal;
static int colorBurst;

#define NTSC_NUM_PHASES     4
#define NTSC_NUM_SEQUENCES  4096
#define NTSC_PIXEL_WIDTH    4
static unsigned char NTSCMono[NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCColor[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCMonoTV[NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];
static unsigned char NTSCColorTV[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES][NTSC_PIXEL_WIDTH];

#define SIGZEROS 2
#define SIGPOLES 2
#define SIGGAIN  7.614490548e+00

static double signal_prefilter (double z)
{
	static double xv[SIGZEROS+1];
	static double yv[SIGPOLES+1];

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
#define CYCLESTART (PI * 4.0 / 16.0)

static void filterloop (void)
{
	int p,s,t,n;
	double z,y0,y1,c,i,q,r,g,b;
	double phi,zz;
	
	for (p = 0; p < 4; ++p)
	{
		phi = p * PI / 2.0 + CYCLESTART;
		for (s = 0; s < 4096; ++s)
		{
			t = s;
			y0 = y1 = c = i = q = 0.0;

			for (n = 0; n < 12; ++n)
			{
				z = (double)(0 != (t & 0x800));
				t = t << 1;
#if 1
				//z = z * 1.25;

				zz = signal_prefilter(z);
				c = chroma_filter(zz);
				y0 = luma0_filter(zz);
				y1 = luma1_filter(zz - c);

				c = c * 2.0;
				i = i + (c * cos(phi) - i) / 8.0;
				q = q + (c * sin(phi) - q) / 8.0;

				phi += (PI / 4);
				if (fabs((2 * PI) - phi) < 0.001) phi = phi - 2 * PI;
				
				zz = signal_prefilter(z);
				c = chroma_filter(zz);
				y0 = luma0_filter(zz);
				y1 = luma1_filter(zz - c);

				c = c * 2.0;
				i = i + (c * cos(phi) - i) / 8.0;
				q = q + (c * sin(phi) - q) / 8.0;

				phi += (PI / 4);
				if (fabs((2 * PI) - phi) < 0.001) phi = phi - 2 * PI;
#else
				y = y + (z - y) / 4.0;
				c = z - y;
				c = c * 2.0;

				i = i + (c * cos(phi) - i) / 8.0;
				q = q + (c * sin(phi) - q) / 8.0;
				phi += (PI / 4);
				if (fabs((2 * PI) - phi) < 0.001) phi = phi - 2 * PI;
				
				y = y + (z - y) / 4.0;
				c = z - y;
				c = c * 2.0;
				
				i = i + (c * cos(phi) - i) / 8.0;
				q = q + (c * sin(phi) - q) / 8.0;
				phi += (PI / 4);
				if (fabs((2 * PI) - phi) < 0.001) phi = phi - 2 * PI;
#endif
			}

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

	filterloop();

	vbp0 = wsLines[0];
}

static unsigned vc = 0;
static unsigned hc = 0;

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
	return vc >= 192 && vc < 262;
}

unsigned char wsVideoByte (unsigned long cycle)
{
	unsigned char * mem;
	mem = MemGetMainPtr(wsVideoAddress[hc]);
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
	hc = 0; \
	if (vc < 192) VIDEO_DRAW_ENDLINE(); \
	if (++vc == 262) \
	{ \
		vc = 0; \
		if (++wsFlashidx == 16) \
		{ \
			wsFlashidx = 0; \
			wsFlashmask ^= 0xffff; \
		} \
	} \
	if (vc < 192) \
	{ \
		vbp0 = wsLines[2*vc]; \
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
		wsVideoAddress[hc] = ad;

		if (hc < 16 && hc >= 12)
		{
			if (colorBurst > 0)
				colorBurst -= 1;
		}
		else if (vc < 192)
		{
			if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);

				bt = hgrbits[csbits[wsVideoCharSet][main[0]][vc & 7]];
				if (0 == wsVideoCharSet && 0x40 == (main[0] & 0xC0))
					bt ^= wsFlashmask;
				VIDEO_DRAW_BITS();
			}
		}
			
		/* increment scanner */
		if (++hc == 65)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblText (long ticks)
{
	unsigned int ad, bt, mbt, abt;

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (hc < 16 && hc >= 12)
		{
			if (colorBurst > 0)
				colorBurst -= 1;
		}
		else if (vc < 192)
		{
			if (hc >= 25)
			{
				unsigned char * aux = MemGetAuxPtr(ad);
				unsigned char * main = MemGetMainPtr(ad);
				
				mbt = csbits[wsVideoCharSet][main[0]][vc & 7];
				if (0 == wsVideoCharSet && 0x40 == (main[0] & 0xC0)) mbt ^= wsFlashmask;

				abt = csbits[wsVideoCharSet][aux[0]][vc & 7];
				if (0 == wsVideoCharSet && 0x40 == (aux[0] & 0xC0)) abt ^= wsFlashmask;

				bt = (mbt << 7) | abt;
				VIDEO_DRAW_BITS();
			}
		}
			
		/* increment scanner */
		if (++hc == 65)
			END_OF_LINE();
	}
}

#if 0
void wsUpdateVideoLores (long ticks)
{
	unsigned ad, bt, uf, txt;
	unsigned x, y;
	
	bt = 0;
	txt = 0;
	if (wsVideoMixed && vc >= 160) txt = 1;
	ad = txttab[vc] + (wsVideoPage * 0x400) + hc;
	
	for (; ticks; --ticks)
	{
#if 0
		uf = 0;
#endif
		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				wsVideoByte = main[0];

				if (txt) {
					bt = hgrbits[csbits[(wsVideoByte * 8) + (vc & 7)]];
					if (0x40 == (wsVideoByte & 0xC0)) {
						bt ^= wsFlashmask;
#if 0
						if (0 == wsFlashidx)
							if (0 == wsTouched[ad])
								wsTouched[ad] = 8;
#endif
					}
				}
				else {
					bt = grbits[(wsVideoByte >> (vc & 4)) & 0xF] >> ((1 - (hc & 1)) * 2);
				}
#if 0
				uf = 0; /* have a new byte, don't know if it will be drawn yet */
				if (1 || wsTouched[ad])
				{
					vbp0 = wsLines[vc] + (hc - 25) * 56;
					
					x = 14 * (hc - 25) + 39;
					y = 2 * vc + 48;
					if (x < dirty.ulx) dirty.ulx = x;
					if (y < dirty.uly) dirty.uly = y;
					x = x + 14;
					y = y + 2;
					if (x > dirty.lrx) dirty.lrx = x;
					if (y > dirty.lry) dirty.lry = y;
				
					sbits = sbitstab[vc][hc];
#endif				
					VIDEO_DRAW_BITS();
#if 0
					if (sbits != sbitstab[vc][hc+1])
					{
						sbitstab[vc][hc+1] = sbits;
						if (hc < 64) wsTouched[ad+1] = 8;
					}
					
					wsTouched[ad] = wsTouched[ad] - 1;
					uf = 1;	/* drew the byte */
				}
#endif
			}
		}
		
		ad = ad + 1;
		
		/* increment scanner */
		if (65 == ++hc)
		{
			/*** end of line ***/
			if (vc < 192)
			{
#if 0
				if (uf)
				{
					/* if last byte on line was drawn, then do this */
					
					x = x + 14;
					y = y + 2;
					if (x > dirty.lrx) dirty.lrx = x;
					if (y > dirty.lry) dirty.lry = y;
#endif
					VIDEO_DRAW_ENDLINE();
#if 0
				}
#endif
			}
		
#if 0
			bt = 0;	/* make sure lastsignal = 0 below if about to exit */
#endif
			hc = 0;
			vc = vc + 1;
			if (vc == 262) {
				vc = 0;
				wsFlashidx += 1;
				if (wsFlashidx == 16) {
					wsFlashidx = 0;
					wsFlashmask ^= 0xffff;
				}
			}
			colorPhase = INITIAL_COLOR_PHASE;
			colorBurst = 0;
			sbits = 0;
			
			txt = 0;
			if (wsVideoMixed && vc >= 160) txt = 1;
			
			ad = txttab[vc] + (wsVideoPage * 0x400);
		}
	}
#if 0
	/* save last signal in case next byte displayed is a shifted hires byte */
	if (uf)
		lastsignal = (bt & 1);	/* drew a byte last time through loop - use last signal */
	else
		lastsignal = (bt & 0x2000) >> 13;	/* what would have been the last signal, but didn't draw it */
#endif
}
#endif

void wsUpdateVideo7MLores (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = hgrbits[0xFF & grbits[(main[0] >> (vc & 4)) & 0xF] >> ((1 - (hc & 1)) * 2)];
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
		if (65 == ++hc)
			END_OF_LINE();
	}
}

void wsUpdateVideoLores (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = grbits[(main[0] >> (vc & 4)) & 0xF] >> ((1 - (hc & 1)) * 2);
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
		if (65 == ++hc)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblLores (long ticks)
{
	unsigned ad, bt, abt, mbt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = TEXT_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * aux = MemGetAuxPtr(ad);
				unsigned char * main = MemGetMainPtr(ad);

				abt = grbits[(aux[0] >> (vc & 4)) & 0xF] >> (((1 - (hc & 1)) * 2) + 3);
				mbt = grbits[(main[0] >> (vc & 4)) & 0xF] >> (((1 - (hc & 1)) * 2) + 3);
				bt = (mbt << 7) | (abt & 0x7f);
			
				VIDEO_DRAW_BITS();
				lastsignal = bt & 1;
			}
		}
		
		/* increment scanner */
		if (65 == ++hc)
			END_OF_LINE();
	}
}

void wsUpdateVideoDblHires (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}

	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
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
		if (65 == ++hc)
			END_OF_LINE();
	}
}

void wsUpdateVideoHires (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);

				bt = hgrbits[main[0]];
				if (main[0] & 0x80) bt = (bt << 1) | lastsignal;
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
		if (65 == ++hc)
			END_OF_LINE();
	}
}

void wsUpdateVideoHires0 (long ticks)
{
	unsigned ad, bt;
	
	if (wsVideoMixed && vc >= 160)
	{
		wsVideoText(ticks);
		return;
	}
	
	for (; ticks; --ticks)
	{
		ad = HGR_ADDRESS();
		wsVideoAddress[hc] = ad;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				bt = hgrbits[main[0]];
				VIDEO_DRAW_BITS();
			}
		}
		
		/* increment scanner */
		if (65 == ++hc)
			END_OF_LINE();
	}
}

#if 0
void wsUpdateVideoHires (long ticks)
{
	unsigned ad, bt, uf, txt;
	unsigned x, y;
	
	bt = 0;

	if (wsVideoMixed && vc >= 160) {
		txt = 1;
		ad = txttab[vc] + (wsVideoPage * 0x400) + hc;
	}
	else {
		txt = 0;
		ad = hgrtab[vc] + (wsVideoPage * 0x2000) + hc;
	}
	
	for (; ticks; --ticks)
	{
		uf = 0;

		if (vc < 192)
		{
			if (hc < 16 && hc >= 12)
			{
				colorBurst = 1024;
			}
			else if (hc >= 25)
			{
				unsigned char * main = MemGetMainPtr(ad);
				wsVideoByte = main[0];

				if (txt) {
					bt = csbits[(8 * wsVideoByte) + (vc % 8)];
					bt = hgrbits[bt];
					if (0x40 == (wsVideoByte & 0xC0)) {
						bt ^= wsFlashmask;
						if (0 == wsFlashidx)
							if (0 == wsTouched[ad])
								wsTouched[ad] = 8;
					}
				}
				else {
					bt = hgrbits[wsVideoByte];
				}
#if 0
				if (1 || wsTouched[ad])
				{
					x = 14 * (hc - 25) + 39;
					y = 2 * vc + 48;
					if (x < dirty.ulx) dirty.ulx = x;
					if (y < dirty.uly) dirty.uly = y;
					x = x + 14;
					y = y + 2;
					if (x > dirty.lrx) dirty.lrx = x;
					if (y > dirty.lry) dirty.lry = y;
					
					sbits = sbitstab[vc][hc];
					//colorPhase = INITIAL_COLOR_PHASE + (1 - (hc & 1)) * 2;
#endif
					vbp0 = wsLines[vc] + (hc - 25) * 56;
					if (txt == 0 && wsVideoByte & 0x80) bt = (bt << 1) | lastsignal;
					VIDEO_DRAW_BITS();
					lastsignal = bt & 1;
#if 0
					if (sbits != sbitstab[vc][hc+1])
					{
						sbitstab[vc][hc+1] = sbits;
						if (hc < 64) wsTouched[ad+1] = 8;
					}
					
					if (txt) wsTouched[ad] = wsTouched[ad] - 1;
					else wsTouched[ad] = 0;
					uf = 1; /* drew the byte */
				}
#endif
			}
		}
		
		ad = ad + 1;
		
		/* increment scanner */
		if (65 == ++hc)
		{
			/*** end of line ***/
			if (vc < 192)
			{
#if 0
				if (uf)
				{
					/* if last byte on line was drawn, then do this */
					
					x = x + 14;
					y = y + 2;
					if (x > dirty.lrx) dirty.lrx = x;
					if (y > dirty.lry) dirty.lry = y;
#endif
					VIDEO_DRAW_ENDLINE();
#if 0
				}
#endif
			}
			
			bt = 0;	/* make sure lastsignal = 0 below if about to exit */
			hc = 0;
			vc = vc + 1;
			if (vc == 262) {
				vc = 0;
				wsFlashidx += 1;
				if (wsFlashidx == 16) {
					wsFlashidx = 0;
					wsFlashmask ^= 0xffff;
				}
			}
			colorPhase = INITIAL_COLOR_PHASE;
			colorBurst = 0;
			sbits = 0;
			
			if (wsVideoMixed && vc >= 160) {
				txt = 1;
				ad = txttab[vc] + (wsVideoPage * 0x400);
			}
			else {
				txt = 0;
				ad = hgrtab[vc] + (wsVideoPage * 0x2000);
			}
		}
	}
#if 0
	/* save last signal in case next byte displayed is a shifted hires byte */
	if (uf)
		lastsignal = (bt & 1);	/* drew a byte last time through loop - use last signal */
	else
		lastsignal = (bt & 0x2000) >> 13;	/* what would have been the last signal, but didn't draw it */
#endif
}
#endif

void (* wsVideoText)(long) = wsUpdateVideoText;
void (* wsVideoUpdate)(long) = wsUpdateVideoText;
