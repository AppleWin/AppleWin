/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: Debugger Heatmap
 *
 * Author: Copyright (C) 2020 Cyril Lambin
 */

#include "StdAfx.h"

#include "Debug.h"
#include "Debugger_Display.h"
#include "Debugger_Heatmap.h"

#include "../Applewin.h"
#include "../CPU.h"
#include "../Frame.h"
#include "../Memory.h"
#include "../NTSC.h"
#include "../Video.h"


//========================================================================================

void DrawMemHeatmap(Update_t bUpdate)
{
	bgra_t* g_pDebuggerExtraFramebits = GetpDebuggerExtraFramebits();
	if (g_pDebuggerExtraFramebits == NULL) return;

	int x = 0;
	int y = 0;
	int color;
	int linesize = FRAMEBUFFER_W;
	int index = 0, ramindex = 0;
	int page = 0, i = 0;
	LPBYTE _rammain = mem;
	LPBYTE _ramaux = mem;

	// Drawing heatmaps

	// Population count (counting number of 1 in a byte) with luma shift for the map
	// TODO: initialize once when debugger is created
	char popcount[256];
	popcount[0] = 0;
	for (x = 1; x < 256; x++) {
		popcount[x] = 7;
		y = x;
		for (; y != 0; y >>= 1)
			if (y & 1)
				popcount[x]++;
		popcount[x] <<= 2;
	}

	// copy back dirty pages into memory banks buffers
	_rammain = MemGetBankPtr(0);
	_ramaux = MemGetBankPtr(1);  // AUX only, we don't handle extra RAM cards (AppleWorks etc)

	int heatmap_diff = g_iMemoryHeatmapValue - 0x1FFFF;

	uint8_t blue;
	for (page = 0; page < 256; page++) {
		for (i = 0; i < 256; i++)
		{
			x = i;
			y = page;
			ramindex = y * 256 + i;
			color = popcount[_rammain[ramindex]];
			index = (383 - HEATMAP_TOPMARGIN - y) * linesize + x + HEATMAP_MAIN_LEFTMARGIN;

			g_aMemoryHeatmap_W[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_W[ramindex] < 0) g_aMemoryHeatmap_W[ramindex] = 0;
			g_aMemoryHeatmap_R[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_R[ramindex] < 0) g_aMemoryHeatmap_R[ramindex] = 0;
			g_aMemoryHeatmap_X[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_X[ramindex] < 0) g_aMemoryHeatmap_X[ramindex] = 0;

			blue = ((g_aMemoryHeatmap_R[ramindex]) >> 9) & 0xFF;
			g_pDebuggerExtraFramebits[index].r = ((g_aMemoryHeatmap_W[ramindex]) >> 9) & 0xFF | color;
			g_pDebuggerExtraFramebits[index].g = (((g_aMemoryHeatmap_X[ramindex]) >> 9) & 0xFF) | (blue >> 1) | color;
			g_pDebuggerExtraFramebits[index].b = blue | color;

			color = popcount[_ramaux[ramindex]];
			index = (383 - HEATMAP_TOPMARGIN - y) * linesize + x + HEATMAP_AUX_LEFTMARGIN;
			ramindex += 0x10000;

			g_aMemoryHeatmap_W[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_W[ramindex] < 0) g_aMemoryHeatmap_W[ramindex] = 0;
			g_aMemoryHeatmap_R[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_R[ramindex] < 0) g_aMemoryHeatmap_R[ramindex] = 0;
			g_aMemoryHeatmap_X[ramindex] -= heatmap_diff;
			if (g_aMemoryHeatmap_X[ramindex] < 0) g_aMemoryHeatmap_X[ramindex] = 0;

			blue = ((g_aMemoryHeatmap_R[ramindex]) >> 9) & 0xFF;
			g_pDebuggerExtraFramebits[index].r = ((g_aMemoryHeatmap_W[ramindex]) >> 9) & 0xFF | color;
			g_pDebuggerExtraFramebits[index].g = (((g_aMemoryHeatmap_X[ramindex]) >> 9) & 0xFF) | (blue >> 1) | color;
			g_pDebuggerExtraFramebits[index].b = blue | color;
		}
	}
	g_iMemoryHeatmapValue = 0x1FFFF;

	// Drawing RAM access bars

	#define COLOR_READ  0x00FF7000  // light blue
	#define COLOR_WRITE 0x000000FF	// red

	int addr, addrR, addrW;
	addr = 0x0000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0x0000) * COLOR_READ | (addrW == 0x0000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x00, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x02, g_pDebuggerExtraFramebits);
	color = (addrR == 0x10000) * COLOR_READ | (addrW == 0x10000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x00, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x02, g_pDebuggerExtraFramebits);

	addr = 0x0400;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0x0400) * COLOR_READ | (addrW == 0x0400) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x04, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x08, g_pDebuggerExtraFramebits);
	color = (addrR == 0x10400) * COLOR_READ | (addrW == 0x10400) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x04, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x08, g_pDebuggerExtraFramebits);

	addr = 0x0200;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0x0200) * COLOR_READ | (addrW == 0x0200) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x02, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x04, g_pDebuggerExtraFramebits);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x08, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x20, g_pDebuggerExtraFramebits);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x60, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0xC0, g_pDebuggerExtraFramebits);
	color = (addrR == 0x10200) * COLOR_READ | (addrW == 0x10200) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x02, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x04, g_pDebuggerExtraFramebits);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x08, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x20, g_pDebuggerExtraFramebits);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x60, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0xC0, g_pDebuggerExtraFramebits);

	addr = 0x2000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0x2000) * COLOR_READ | (addrW == 0x2000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x20, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x40, g_pDebuggerExtraFramebits);
	color = (addrR == 0x12000) * COLOR_READ | (addrW == 0x12000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x20, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x40, g_pDebuggerExtraFramebits);

	addr = 0x4000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0x4000) * COLOR_READ | (addrW == 0x4000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0x40, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x60, g_pDebuggerExtraFramebits);
	color = (addrR == 0x14000) * COLOR_READ | (addrW == 0x14000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x40, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x60, g_pDebuggerExtraFramebits);

	addr = 0xC000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0xC000) * COLOR_READ | (addrW == 0xC000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0xC0, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0xD0, g_pDebuggerExtraFramebits);
	color = (addrR == 0x1C000) * COLOR_READ | (addrW == 0x1C000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0xC0, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0xD0, g_pDebuggerExtraFramebits);

	addr = 0xD000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0xC000) * COLOR_READ | (addrW == 0xC000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0xC0, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0xD0, g_pDebuggerExtraFramebits);
	color = (addrR == 0xD000) * COLOR_READ | (addrW == 0xD000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0xD0, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0xE0, g_pDebuggerExtraFramebits);
	color = (addrR == 0x1C000) * COLOR_READ | (addrW == 0x1C000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0xC0, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0xD0, g_pDebuggerExtraFramebits);
	color = (addrR == 0x1D000) * COLOR_READ | (addrW == 0x1D000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0xD0, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0xE0, g_pDebuggerExtraFramebits);

	addr = 0xE000;
	addrR = MemGetBank(addr, false);
	addrW = MemGetBank(addr, true);
	color = (addrR == 0xE000) * COLOR_READ | (addrW == 0xE000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256, HEATMAP_TOPMARGIN + 0xE0, HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x100, g_pDebuggerExtraFramebits);
	color = (addrR == 0x1E000) * COLOR_READ | (addrW == 0x1E000) * COLOR_WRITE;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0xE0, HEATMAP_AUX_LEFTMARGIN, HEATMAP_TOPMARGIN + 0x100, g_pDebuggerExtraFramebits);

	// Drawing Video display bars

	// TEXT or GR or MIXED + PAGE2OFF or 80STOREON
	color = ((VideoGetSWTEXT() || !VideoGetSWHIRES() || VideoGetSWMIXED()) && (VideoGetSW80STORE() || !VideoGetSWPAGE2())) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x04, HEATMAP_MAIN_LEFTMARGIN + 256 + 8, HEATMAP_TOPMARGIN + 0x08, g_pDebuggerExtraFramebits);
	// 80COL or DGR + PAGE2OFF or 80STOREON 
	color = (((VideoGetSWTEXT() && VideoGetSW80COL()) || (!VideoGetSWTEXT() && !VideoGetSWHIRES() || VideoGetSWMIXED()) && VideoGetSWDHIRES()) && (VideoGetSW80STORE() || !VideoGetSWPAGE2())) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 8, HEATMAP_TOPMARGIN + 0x04, HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x08, g_pDebuggerExtraFramebits);
	// TEXT or GR or MIXED + PAGE2ON + 80STOREOFF
	color = ((VideoGetSWTEXT() || !VideoGetSWHIRES() || VideoGetSWMIXED()) && !VideoGetSW80STORE() && VideoGetSWPAGE2()) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x08, HEATMAP_MAIN_LEFTMARGIN + 256 + 8, HEATMAP_TOPMARGIN + 0x0C, g_pDebuggerExtraFramebits);
	// 80COL or DGR + PAGE2ON + 80STOREOFF
	color = (((VideoGetSWTEXT() && VideoGetSW80COL()) || (!VideoGetSWTEXT() && !VideoGetSWHIRES() || VideoGetSWMIXED()) && VideoGetSWDHIRES()) && !VideoGetSW80STORE() && VideoGetSWPAGE2()) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 8, HEATMAP_TOPMARGIN + 0x08, HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x0C, g_pDebuggerExtraFramebits);

	// HGR + PAGE2OFF or 80STOREON
	color = (!VideoGetSWTEXT() && VideoGetSWHIRES() && (VideoGetSW80STORE() || !VideoGetSWPAGE2())) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x20, HEATMAP_MAIN_LEFTMARGIN + 256 + 8, HEATMAP_TOPMARGIN + 0x40, g_pDebuggerExtraFramebits);
	// DHGR + PAGE2OFF or 80STOREON 
	color = (!VideoGetSWTEXT() && VideoGetSWHIRES() && VideoGetSWDHIRES() && (VideoGetSW80STORE() || !VideoGetSWPAGE2())) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 8, HEATMAP_TOPMARGIN + 0x20, HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x40, g_pDebuggerExtraFramebits);
	// HGR + PAGE2ON + 80STOREOFF
	color = (!VideoGetSWTEXT() && VideoGetSWHIRES() && !VideoGetSW80STORE() && VideoGetSWPAGE2()) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_MAIN_LEFTMARGIN + 256 + 4, HEATMAP_TOPMARGIN + 0x40, HEATMAP_MAIN_LEFTMARGIN + 256 + 8, HEATMAP_TOPMARGIN + 0x60, g_pDebuggerExtraFramebits);
	// DHGR + PAGE2ON + 80STOREOFF
	color = (!VideoGetSWTEXT() && VideoGetSWHIRES() && VideoGetSWDHIRES() && !VideoGetSW80STORE() && VideoGetSWPAGE2()) * 0x00FFFFFF;
	DebuggerSetColorBG(color);
	FillBackground(HEATMAP_AUX_LEFTMARGIN - 8, HEATMAP_TOPMARGIN + 0x40, HEATMAP_AUX_LEFTMARGIN - 4, HEATMAP_TOPMARGIN + 0x60, g_pDebuggerExtraFramebits);

}

