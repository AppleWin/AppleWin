/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms
Copyright (C) 2016, Tom Charlesworth

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
#include "Applewin.h"
#include "Video.h"

#include "NTSC_CharSet.h"

unsigned char csbits_enhanced2e[2][256][8];	// Enhanced //e (2732 4K video ROM)
static unsigned char csbits_2e_pal[2][256][8];	// PAL Original or Enhanced //e (2764 8K video ROM - low 4K) via rocker switch under keyboard
unsigned char csbits_2e[2][256][8];			// Original //e (no mousetext)
unsigned char csbits_a2[1][256][8];			// ][ and ][+
unsigned char csbits_pravets82[1][256][8];	// Pravets 82
unsigned char csbits_pravets8M[1][256][8];	// Pravets 8M
unsigned char csbits_pravets8C[2][256][8];	// Pravets 8A & 8C

//

static const UINT bitmapWidth = 256;
static const UINT bitmapWidthBytes = bitmapWidth/8;
static const UINT bitmapHeight = 768;

static const UINT charWidth = 16;
static const UINT charWidthBytes = 16/8;
static const UINT charHeight = 16;

static void get_csbits_xy(csbits_t csbits, UINT ch, UINT cx, UINT cy, const BYTE* pBitmap)
{
	_ASSERT(ch < 256);
	_ASSERT((cx < bitmapWidth/charWidth) && (cy < bitmapHeight/charHeight));

	pBitmap += cy*charHeight*bitmapWidthBytes + cx*charWidthBytes;

	for (UINT y=0; y<8; y++)
	{
		BYTE n = 0;
		for (int x=0; x<14; x+=2)
		{
			UINT xp = x/8;
			BYTE d = pBitmap[xp];
			UINT b = 7 - x%8;
			if (d & (1<<b)) n |= 0x80;
			n >>= 1;
		}

		csbits[0][ch][y] = n;
		pBitmap += bitmapWidthBytes*2;
	}
}

static void get_csbits(csbits_t csbits, const char* resourceName, const UINT cy0)
{
	const UINT bufferSize = bitmapWidthBytes*bitmapHeight;
	BYTE* pBuffer = new BYTE [bufferSize];

	HBITMAP hCharBitmap = LoadBitmap(g_hInstance, resourceName);
	GetBitmapBits(hCharBitmap, bufferSize, pBuffer);

	for (UINT cy=cy0, ch=0; cy<cy0+16; cy++)
	{
		for (UINT cx=0; cx<16; cx++)
		{
			get_csbits_xy(csbits, ch++, cx, cy, pBuffer);
		}
	}

	DeleteObject(hCharBitmap);

	delete [] pBuffer;
}

//-------------------------------------

// ROM address (RA):
// -----------------
// . RA10,..,RA3;SEGC,SEGB,SEGA => [2^8][2^3] => 256 chars of 8 lines (total = 2KiB)
// . VID7,..,VID0 is the 8-bit video character (eg. from TEXT/$400 memory)
//
// UTAIIe:8-13, Table 8.2:
//
// ALTCHRSET | RA10              | RA9
//------------------------------------------
//     0     | VID7 + VID6.FLASH | VID6.VID7
//     1     | VID7              | VID6
//
// FLASH toggles every 16 VBLs, so alternates between selecting NORMAL control/special and INVERSE control/special
//

void userVideoRom4K(csbits_t csbits, const BYTE* pVideoRom)
{
	int RA = 0;	// rom address
	int i = 0;

	// regular char set

	for (; i<64; i++, RA+=8)		// [00..3F] INVERSE / [40..7F] FLASH
	{
		for (int y=0; y<8; y++)
		{
			csbits[0][i][y]    = pVideoRom[RA+y] ^ 0xff;	// UTAIIe:8-11 "dot patterns in the video ROM are inverted..."
			csbits[0][i+64][y] = pVideoRom[RA+y] ^ 0xff;	// UTAIIe:8-14 (Table 8.3) we use FLASH=0, so RA=00ccccccsss
		}
	}

	RA = (1<<10 | 0<<9);									// UTAIIe:8-14 (Table 8.3)

	for (i=128; i<256; i++, RA+=8)	// [80..BF] NORMAL
	{
		for (int y=0; y<8; y++)
		{
			csbits[0][i][y] = pVideoRom[RA+y] ^ 0xff;		// UTAIIe:8-11 "dot patterns in the video ROM are inverted..."
		}
	}

	RA = (1<<10 | 1<<9);									// UTAIIe:8-14 (Table 8.3)

	for (i=192; i<256; i++, RA+=8)	// [C0..FF] NORMAL
	{
		for (int y=0; y<8; y++)
		{
			csbits[0][i][y] = pVideoRom[RA+y] ^ 0xff;		// UTAIIe:8-11 "dot patterns in the video ROM are inverted..."
		}
	}

	// alt char set

	RA = 0;

	for (i=0; i<256; i++, RA+=8)	// [00..7F] INVERSE / [80..FF] NORMAL
	{
		for (int y=0; y<8; y++)
		{
			csbits[1][i][y] = pVideoRom[RA+y] ^ 0xff;		// UTAIIe:8-11 "dot patterns in the video ROM are inverted..."
		}
	}
}

void userVideoRomForIIe(void)
{
	const BYTE* pVideoRom;
	UINT size = GetVideoRom(pVideoRom);	// 2K or 4K or 8K
	if (size < kVideoRomSize4K)
		return;

	if (size == kVideoRomSize4K)
	{
		userVideoRom4K(&csbits_enhanced2e[0], pVideoRom);
	}
	else
	{
		userVideoRom4K(&csbits_2e_pal[0], pVideoRom);
		userVideoRom4K(&csbits_enhanced2e[0], &pVideoRom[4*1024]);
	}

	// NB. Same *custom* US video ROM for Original & Enhanced //e
	memcpy(csbits_2e, csbits_enhanced2e, sizeof(csbits_enhanced2e));
}

//-------------------------------------

void userVideoRom2K(csbits_t csbits, const BYTE* pVideoRom)
{
	int RA = 0;	// rom address

	for (int i=0; i<256; i++, RA+=8)
	{
		for (int y=0; y<8; y++)
		{
			BYTE n = pVideoRom[RA+y];

			// UTAII:8-30 "Bit 7 of your EPROM fonts will control flashing in the lower 1024 bytes of the EPROM"
			// UTAII:8-31 "If you leave O7 (EPROM Output7) reset in these patterns, the resulting characters will be inversions..."
			if (!(n & 0x80) && RA < 1024)
				n = n ^ 0x7f;

			// UTAII:8-30 "TEXT ROM pattern is ... reversed"
			BYTE d = 0;
			for (BYTE j=0; j<7; j++, n >>= 1)	// Just bits [0..6]
				d = (d << 1) | (n & 1);

			csbits[0][i][y] = d;
		}
	}
}

void userVideoRomForIIPlus(void)
{
	const BYTE* pVideoRom;
	UINT size = GetVideoRom(pVideoRom);	// 2K or 4K or 8K
	if (size != kVideoRomSize2K)
		return;

	userVideoRom2K(&csbits_a2[0], pVideoRom);
}

//-------------------------------------

void make_csbits(void)
{
	get_csbits(&csbits_enhanced2e[0], TEXT("CHARSET40"), 0);	// Enhanced //e: Alt char set off
	get_csbits(&csbits_enhanced2e[1], TEXT("CHARSET40"), 16);	// Enhanced //e: Alt char set on (mousetext)
	get_csbits(&csbits_a2[0],         TEXT("CHARSET40"), 32);	// Apple ][, ][+
	get_csbits(&csbits_pravets82[0],  TEXT("CHARSET82"), 0);	// Pravets 82
	get_csbits(&csbits_pravets8M[0],  TEXT("CHARSET8M"), 0);	// Pravets 8M
	get_csbits(&csbits_pravets8C[0],  TEXT("CHARSET8C"), 0);	// Pravets 8A / 8C: Alt char set off
	get_csbits(&csbits_pravets8C[1],  TEXT("CHARSET8C"), 16);	// Pravets 8A / 8C: Alt char set on

	// Original //e is just Enhanced //e with the 32 mousetext chars [0x40..0x5F] replaced by the non-alt charset chars [0x40..0x5F]
	memcpy(csbits_2e, csbits_enhanced2e, sizeof(csbits_enhanced2e));
	memcpy(&csbits_2e[1][64], &csbits_2e[0][64], 32*8);

	// Try to use any user-provided video ROM for Original/Enhanced //e
	userVideoRomForIIe();

	// Try to use any user-provided video ROM for II/II+
	userVideoRomForIIPlus();
}

csbits_t Get2e_csbits(void)
{
	const csbits_t videoRom4K = (GetApple2Type() == A2TYPE_APPLE2E) ? csbits_2e : csbits_enhanced2e;

	if (IsVideoRom4K())	// 4K means US-only, so no secondary PAL video ROM
		return videoRom4K;

	return GetVideoRomRockerSwitch() == false ? videoRom4K : csbits_2e_pal;	// NB. Same PAL video ROM for Original & Enhanced //e
}
