/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2021, Tom Charlesworth, Michael Pohoreski

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
/*
  Emulate a VidHD card (Blue Shift Inc)

  Allows any Apple II to support the IIgs' 320x200 and 640x200 256-colour Super High-Res video modes.
  Currently only a //e with 64K aux memory supports SHR mode.

  NB. The extended text modes 80x45, 120x67, 240x135 (and setting FG/BG colours) are not supported yet.
*/

#include "StdAfx.h"

#include "Memory.h"
#include "NTSC.h"
#include "Video.h"
#include "VidHD.h"
#include "YamlHelper.h"

void VidHDCard::Reset(const bool powerCycle)
{
	m_NEWVIDEO = 0;
	GetVideo().SetVideoMode(GetVideo().GetVideoMode() & ~VF_SHR);
}

void VidHDCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, IO_Null, IO_Null, &VidHDCard::IORead, IO_Null, this, NULL);
}

BYTE __stdcall VidHDCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	// Return magic bytes (from the VidHD firmware) for VidHD detection
	switch (addr & 0xff)
	{
	case 0: return 0x24;
	case 1: return 0xEA;
	case 2: return 0x4C;
	}
	return IO_Null(pc, addr, bWrite, value, nExecutedCycles);
}

// NB. VidHD has no support for reading the IIgs video registers (from an earlier Apple II)
void VidHDCard::VideoIOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	switch (addr & 0xff)
	{
	case 0x22:	// SCREENCOLOR
		m_SCREENCOLOR = value;
		break;
	case 0x29:	// NEWVIDEO
		m_NEWVIDEO = value;
		break;
	case 0x34:	// BORDERCOLOR
		m_BORDERCOLOR = value;
		break;
	case 0x35:	// SHADOW
		m_SHADOW = value;
		break;
	default:
		_ASSERT(0);
	}
}

//===========================================================================

#pragma pack(push)
#pragma pack(1)	// Ensure struct is packed
struct Color
{
	USHORT blue : 4;
	USHORT green : 4;
	USHORT red : 4;
	USHORT reserved : 4;
};
#pragma pack(pop)

bgra_t ConvertIIgs2RGB(Color color)
{
	bgra_t rgb = { 0 };
	rgb.r = color.red * 16;
	rgb.g = color.green * 16;
	rgb.b = color.blue * 16;
	return rgb;
}

void VidHDCard::UpdateSHRCell(bool is640Mode, bool isColorFillMode, uint16_t addrPalette, bgra_t* pVideoAddress, uint32_t a)
{
	_ASSERT(!is640Mode);		// to do: test this mode

	Color* palette = (Color*) MemGetAuxPtr(addrPalette);

	for (UINT i = 0; i < 4; i++)
	{
		if (!is640Mode) // 320 mode
		{
			BYTE pixel1 = (a >> 4) & 0xf;
			bgra_t color1 = ConvertIIgs2RGB(palette[pixel1]);
			if (isColorFillMode && pixel1 == 0) color1 = *(pVideoAddress - 1);
			*pVideoAddress++ = color1;
			*pVideoAddress++ = color1;

			BYTE pixel2 = a & 0xf;
			bgra_t color2 = ConvertIIgs2RGB(palette[pixel2]);
			if (isColorFillMode && pixel2 == 0) color2 = color1;
			*pVideoAddress++ = color2;
			*pVideoAddress++ = color2;
		}
		else // 640 mode - see IIgs Hardware Ref, Pg.96, Table4-21 'Color Selection in 640 mode'
		{
			BYTE pixel1 = (a >> 6) & 0x3;
			bgra_t color1 = ConvertIIgs2RGB(palette[0x8 + pixel1]);
			*pVideoAddress++ = color1;

			BYTE pixel2 = (a >> 4) & 0x3;
			bgra_t color2 = ConvertIIgs2RGB(palette[0xC + pixel2]);
			*pVideoAddress++ = color2;

			BYTE pixel3 = (a >> 2) & 0x3;
			bgra_t color3 = ConvertIIgs2RGB(palette[0x0 + pixel3]);
			*pVideoAddress++ = color3;

			BYTE pixel4 = a & 0x3;
			bgra_t color4 = ConvertIIgs2RGB(palette[0x4 + pixel4]);
			*pVideoAddress++ = color4;
		}

		a >>= 8;
	}
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

#define SS_YAML_KEY_SCREEN_COLOR "Screen Color"
#define SS_YAML_KEY_NEW_VIDEO "New Video"
#define SS_YAML_KEY_BORDER_COLOR "Border Color"
#define SS_YAML_KEY_SHADOW "Shadow"

std::string VidHDCard::GetSnapshotCardName(void)
{
	static const std::string name("VidHD");
	return name;
}

void VidHDCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SCREEN_COLOR, m_SCREENCOLOR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_NEW_VIDEO, m_NEWVIDEO);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_BORDER_COLOR, m_BORDERCOLOR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SHADOW, m_SHADOW);
}

bool VidHDCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	m_SCREENCOLOR = yamlLoadHelper.LoadUint(SS_YAML_KEY_SCREEN_COLOR);
	m_NEWVIDEO = yamlLoadHelper.LoadUint(SS_YAML_KEY_NEW_VIDEO);
	m_BORDERCOLOR = yamlLoadHelper.LoadUint(SS_YAML_KEY_BORDER_COLOR);
	m_SHADOW = yamlLoadHelper.LoadUint(SS_YAML_KEY_SHADOW);

	return true;
}
