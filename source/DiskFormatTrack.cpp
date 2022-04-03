/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2017, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Disk][ format track support
 *
 * Author: Various
 */

/*
DOS 3.2 INIT notes
------------------
Writes the following: (in 1 continuous write operation)
. 0x26A0 x 36-cycle (9-bit) sync FFs - wraps track
. Address Field (Prologue=D5AAB5, Epilogue=DEAAEB)
	. NB. this trailing EB gets overwritten when sector is written to
	. Sector order: {0,A,7,4,1,B,8,5,2,C,9,6,3}
. 432 x 32-cycle FFs
	. Gap2: 10
	. Data Field:3+410+1+3=417
	. NB. no Data Field pro/epilogue written during track format
.  79 x 36-cycle (9-bit) sync FFs (gap3?)
. Address Field (Prologue=D5AAB5, ...), etc.
*/

#include "StdAfx.h"

#include "DiskFormatTrack.h"
#include "Disk.h"
#include "Log.h"
#include "YamlHelper.h"

// Occurs on these conditions:
// . ctor
// . disk][ reset
void FormatTrack::Reset(void)
{
	memset(m_VolTrkSecChk, 0, sizeof(m_VolTrkSecChk));
	memset(m_VolTrkSecChk4and4, 0, sizeof(m_VolTrkSecChk4and4));
	m_trackState = TS_GAP1;
	m_uLast3Bytes = 0;
	m_4and4idx = 0;

	DriveNotWritingTrack();
}

// Occurs on these conditions:
// . spin > 2 bytes (empirical from inspecting DOS3.3-INIT/ProDOS-FORMAT operations)
// . drive stepper track change
// . drive motor state change
// . switch to read mode after having written a complete track
void FormatTrack::DriveNotWritingTrack(void)
{
	m_bmWrittenSectorAddrFields = 0x0000;
	m_WriteTrackStartIndex = 0;
	m_WriteTrackHasWrapped = false;
	m_WriteDataFieldPrologueCount = 0;
	m_bAddressPrologueIsDOS3_2 = false;

#if LOG_DISK_NIBBLES_WRITE_TRACK_GAPS
	m_DbgGap1Size = 0;
	m_DbgGap2Size = 0;
	m_DbgGap3Size = -1;	// Data Field's epilogue has an extra 0xFF, which isn't part of the Gap3 sync-FF field
#endif
}

void FormatTrack::UpdateOnWriteLatch(UINT uSpinNibbleCount, const FloppyDisk* const pFloppy)
{
	if (pFloppy->m_bWriteProtected)
		return;

	if (m_bmWrittenSectorAddrFields == 0x0000)
	{
		if (m_WriteTrackStartIndex == (UINT)-1)		// waiting for 1st write?
			m_WriteTrackStartIndex = pFloppy->m_byte;
		return;
	}

	// Written at least 1 sector

	if (m_WriteTrackHasWrapped)
		return;

	if (uSpinNibbleCount > 2)
	{
		_ASSERT(0);	// Not seen this case yet
		DriveNotWritingTrack();
		return;
	}

	UINT uTrackIndex = pFloppy->m_byte;
	const UINT& kTrackMaxNibbles = pFloppy->m_nibbles;

	// NB. spin in write mode is only max 1-2 bytes
	do
	{
		if (m_WriteTrackStartIndex == uTrackIndex)	// disk has completed a revolution
		{
			// Occurs for: .dsk & enhance=0|1 & ProDOS-FORMAT/DOS3.3-INIT with big gap3 size (as trackimage is only 0x18F0 in size)
			m_WriteTrackHasWrapped = true;

			// Now wait until drive switched from write to read mode
			return;
		}

		uTrackIndex = (uTrackIndex+1) % kTrackMaxNibbles;
	}
	while (uSpinNibbleCount--);
}

void FormatTrack::DriveSwitchedToReadMode(FloppyDisk* const pFloppy)
{
	if (m_bAddressPrologueIsDOS3_2)
	{
		if (m_bmWrittenSectorAddrFields != 0x1FFF)	// written all 13 sectors? (NB. data field not written by formatter)
			return;
	}
	else
	{
		if (m_bmWrittenSectorAddrFields != 0xFFFF || m_WriteDataFieldPrologueCount != 16)	// written all 16 sectors?
			return;
	}

	// Zero-fill the remainder of the track buffer:
	// Either up to 0x18F0 (if less than 0x18F0) or up to 0x1A00 (for .nib).

	// When there's no spin (enhanced=1), then 0x18F0 is still too long, eg: (see GH#125)
	//  Gap1 = 0x81, Gap2 = 0x05, Gap3 = 0x1D (29), TrackSize = 0x1963
	//  Gap1 = 0x81, Gap2 = 0x05, Gap3 = 0x1C (28), TrackSize = 0x1954
	//  Gap1 = 0x81, Gap2 = 0x05, Gap3 = 0x1B (27), TrackSize = 0x1945
	//  Gap1 = 0x81, Gap2 = 0x05, Gap3 = 0x1A (26), TrackSize = 0x1936
	//
	// And 0x1936 - 0x81(gap1) = 0x18B5
	// So need a track size between 0x18B0 (rounding down) and 0x182F
	const UINT kShortTrackLen = 0x18B0;

	LPBYTE TrackBuffer = pFloppy->m_trackimage;
	const UINT kLongTrackLen = pFloppy->m_nibbles;

	UINT uWriteTrackEndIndex = pFloppy->m_byte;
	UINT uWrittenTrackSize = m_WriteTrackHasWrapped ? kLongTrackLen : 0;

	if (m_WriteTrackStartIndex <= uWriteTrackEndIndex)
		uWrittenTrackSize += uWriteTrackEndIndex - m_WriteTrackStartIndex;
	else
		uWrittenTrackSize += (kLongTrackLen - m_WriteTrackStartIndex) + uWriteTrackEndIndex;

#if LOG_DISK_NIBBLES_WRITE_TRACK_GAPS
	LOG_DISK("Gap1 = 0x%02X, Gap2 = 0x%02X, Gap3 = 0x%02X (%02d), TrackSize = 0x%04X\n", m_DbgGap1Size, m_DbgGap2Size, m_DbgGap3Size, m_DbgGap3Size, uWrittenTrackSize);
#endif

	if (uWrittenTrackSize > kShortTrackLen)
		uWrittenTrackSize = kShortTrackLen;

	int iSrc = uWriteTrackEndIndex - uWrittenTrackSize;	// Rewind to start of non-overwritten part of short track
	if (iSrc < 0)
		iSrc += kLongTrackLen;

	// S < E: |  S-------E         |
	// S > E: |----E           S---|
	if ((UINT)iSrc < uWriteTrackEndIndex)
	{
		memset(&TrackBuffer[0], 0, iSrc);
		memset(&TrackBuffer[uWriteTrackEndIndex], 0, kLongTrackLen - uWriteTrackEndIndex);
	}
	else
	{
		// NB. For the iSrc == uWriteTrackEndIndex case: memset() size=0
		memset(&TrackBuffer[uWriteTrackEndIndex], 0, iSrc - uWriteTrackEndIndex);
	}

	// NB. No need to skip the zero nibbles, as INIT/FORMAT's 'read latch until high bit' loop will just consume these

	DriveNotWritingTrack();
}

void FormatTrack::DriveSwitchedToWriteMode(UINT uTrackIndex)
{
	// ProDOS/DOS3.3: during track write, switches to read mode then back to write mode:
	// . between Address Field & Gap2; and between Data Field & Gap3
	// . so if at TS_GAP2_START then stay at TS_GAP2_START
	if (m_trackState != TS_GAP2_START)
		m_trackState = (m_bmWrittenSectorAddrFields == 0x0000) ? TS_GAP1 : TS_GAP3;
	m_uLast3Bytes = 0;
	m_4and4idx = 0;

	if (m_bmWrittenSectorAddrFields == 0x0000)	// written no sectors
	{
		m_WriteTrackStartIndex = (UINT)-1;		// wait for 1st write 
	}
}

// This is just for debug/logging: used to output when a new Address Field has been read
void FormatTrack::DecodeLatchNibbleRead(BYTE floppylatch)
{
	DecodeLatchNibble(floppylatch, false, false);
}

void FormatTrack::DecodeLatchNibbleWrite(BYTE floppylatch, UINT uSpinNibbleCount, const FloppyDisk* const pFloppy, bool bIsSyncFF)
{
	DecodeLatchNibble(floppylatch, true, bIsSyncFF);
	UpdateOnWriteLatch(uSpinNibbleCount, pFloppy);
}

void FormatTrack::DecodeLatchNibble(BYTE floppylatch, bool bIsWrite, bool bIsSyncFF)
{
	m_uLast3Bytes <<= 8;
	m_uLast3Bytes |= floppylatch;
	m_uLast3Bytes &= 0xFFFFFF;

	if (m_trackState == TS_GAP2_START && bIsWrite)	// NB. bIsWrite, as there's a read between writing Addr Field & Gap2
		m_trackState = TS_GAP2;

#if LOG_DISK_NIBBLES_WRITE_TRACK_GAPS
	if (bIsSyncFF && (m_trackState == TS_GAP1 || m_trackState == TS_GAP2 || m_trackState == TS_GAP3))
	{
		if (m_bmWrittenSectorAddrFields == 0x0000 && m_trackState == TS_GAP1)
			m_DbgGap1Size++;
		else if (m_bmWrittenSectorAddrFields == 0x0001 && m_trackState == TS_GAP2)
			m_DbgGap2Size++;	// Only count Gap2 after sector0 Addr Field (assume other inter-sectors gap2's have same count)
		else if (m_bmWrittenSectorAddrFields == 0x0001 && m_trackState == TS_GAP3)
			m_DbgGap3Size++;	// Only count Gap3 between sector0 & 1 (assume other inter-sectors gap3's have same count)
		return;
	}
#endif

	// Beneath Apple ProDOS 3-14: NB. $D5 and $AA are reserved (never written as data)
	const UINT32 kADDR_PROLOGUE_DOS3_3 = 0xD5AA96;	// D5AA96 is used by DOS3.3/ProDOS
	const UINT32 kADDR_PROLOGUE_DOS3_2 = 0xD5AAB5;	// D5AAB5 is used by DOS3.2/3.2.1
	if (m_uLast3Bytes == kADDR_PROLOGUE_DOS3_3 || m_uLast3Bytes == kADDR_PROLOGUE_DOS3_2)
	{
		m_bAddressPrologueIsDOS3_2 = (m_uLast3Bytes == kADDR_PROLOGUE_DOS3_2);
		m_trackState = TS_ADDRFIELD;
		m_4and4idx = 0;
		return;
	}

	// NB. If TS_ADDRFIELD && m_4and4idx == 8, then writing Address Field's epilogue
	if (m_trackState == TS_ADDRFIELD && m_4and4idx < 8)
	{
		m_VolTrkSecChk4and4[m_4and4idx++] = floppylatch;

		if (m_4and4idx == 8)
		{
			for (UINT i=0; i<4; i++)
				m_VolTrkSecChk[i] = ((m_VolTrkSecChk4and4[i*2] & 0x55) << 1) | (m_VolTrkSecChk4and4[i*2+1] & 0x55);

#if LOG_DISK_NIBBLES_READ
			const bool chk = (m_VolTrkSecChk[0] ^ m_VolTrkSecChk[1] ^ m_VolTrkSecChk[2] ^ m_VolTrkSecChk[3]) == 0;
			if (!bIsWrite)
			{
				BYTE addrPrologue = m_bAddressPrologueIsDOS3_2 ? (BYTE)kADDR_PROLOGUE_DOS3_2 : (BYTE)kADDR_PROLOGUE_DOS3_3;
				m_strReadD5AAxxDetected = StrFormat("read D5AA%02X detected - Vol:%02X Trk:%02X Sec:%02X Chk:%02X %s",
					addrPrologue, m_VolTrkSecChk[0], m_VolTrkSecChk[1], m_VolTrkSecChk[2], m_VolTrkSecChk[3], chk?"":"(bad)");
				if (!m_bSuppressReadD5AAxxDetected)
				{
					LOG_DISK("%s\r\n", m_strReadD5AAxxDetected.c_str());
				}
			}
#endif
#if LOG_DISK_NIBBLES_WRITE
			if (bIsWrite)
			{
				BYTE addrPrologue = m_bAddressPrologueIsDOS3_2 ? (BYTE)kADDR_PROLOGUE_DOS3_2 : (BYTE)kADDR_PROLOGUE_DOS3_3;
				LOG_DISK("write D5AA%02X detected - Vol:%02X Trk:%02X Sec:%02X Chk:%02X %s\r\n", addrPrologue, m_VolTrkSecChk[0], m_VolTrkSecChk[1], m_VolTrkSecChk[2], m_VolTrkSecChk[3], chk?"":"(bad)");
			}
#endif

			if (bIsWrite)
			{
				// NB. Address Field only written when formatting track
				BYTE sector = m_VolTrkSecChk[2];
				_ASSERT( sector <= 15 );
				if (sector > 15)	// Ignore exotic formats with >16 sectors!
					return;
				_ASSERT( (m_bmWrittenSectorAddrFields & (1<<sector)) == 0 );
				m_bmWrittenSectorAddrFields |= (1<<sector);
			}
		}

		return;
	}

#if LOG_DISK_NIBBLES_WRITE_TRACK_GAPS
	if (m_uLast3Bytes == 0xDEAAEB && bIsWrite)	// NB. bIsWrite, as reads could start reading anywhere
	{
		if (m_trackState == TS_ADDRFIELD)
		{
			m_trackState = TS_GAP2_START;
		}
		else // m_trackState == TS_DATAFIELD
		{
			_ASSERT(m_trackState == TS_DATAFIELD);
			m_trackState = TS_GAP3;
		}
		return;
	}
#endif

	if (m_uLast3Bytes == 0xD5AAAD)
	{
		m_trackState = TS_DATAFIELD;
		if (bIsWrite)	// NB. Data Field written every time a sector is written
			m_WriteDataFieldPrologueCount++;
		_ASSERT(m_WriteDataFieldPrologueCount <= 16);
	}
}

//===========================================================================

#define SS_YAML_KEY_FORMAT_TRACK "Format Track state"
// NB. No version - this is determined by the parent "Disk][" unit

#define SS_YAML_KEY_WRITTEN_SECTOR_ADDR_FIELDS "Written Sector Address Fields"
#define SS_YAML_KEY_WRITE_TRACK_START_IDX "Write Track Start Index"
#define SS_YAML_KEY_WRITE_TRACK_HAS_WRAPPED "Write Track Wrapped"
#define SS_YAML_KEY_WRITE_DATA_FIELD_PROLOGUE_COUNT "Write Data Field Prologue Count"
#define SS_YAML_KEY_TRACK_STATE "Track State"
#define SS_YAML_KEY_LAST3BYTES "Last 3 bytes"
#define SS_YAML_KEY_VTSC_4AND4 "Vol,Trk,Sec,Chk (4 and 4)"
#define SS_YAML_KEY_VTSC_4AND4_IDX "Index (4 and 4)"

void FormatTrack::SaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_FORMAT_TRACK);

	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_WRITTEN_SECTOR_ADDR_FIELDS, m_bmWrittenSectorAddrFields);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_WRITE_TRACK_START_IDX, m_WriteTrackStartIndex);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_WRITE_TRACK_HAS_WRAPPED, m_WriteTrackHasWrapped);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_WRITE_DATA_FIELD_PROLOGUE_COUNT, m_WriteDataFieldPrologueCount);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_TRACK_STATE, m_trackState);
	yamlSaveHelper.SaveHexUint24(SS_YAML_KEY_LAST3BYTES, m_uLast3Bytes);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_VTSC_4AND4, *(UINT64*)m_VolTrkSecChk4and4);	// Stored in reverse order
	yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_VTSC_4AND4_IDX, m_4and4idx);
}

void FormatTrack::LoadSnapshot(class YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_FORMAT_TRACK))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_FORMAT_TRACK);

	m_bmWrittenSectorAddrFields   = yamlLoadHelper.LoadUint(SS_YAML_KEY_WRITTEN_SECTOR_ADDR_FIELDS);
	m_WriteTrackStartIndex        = yamlLoadHelper.LoadUint(SS_YAML_KEY_WRITE_TRACK_START_IDX);
	m_WriteTrackHasWrapped        = yamlLoadHelper.LoadBool(SS_YAML_KEY_WRITE_TRACK_HAS_WRAPPED);
	m_WriteDataFieldPrologueCount = yamlLoadHelper.LoadUint(SS_YAML_KEY_WRITE_DATA_FIELD_PROLOGUE_COUNT);
	m_trackState                  = (TRACKSTATE_e) yamlLoadHelper.LoadUint(SS_YAML_KEY_TRACK_STATE);
	m_uLast3Bytes                 = yamlLoadHelper.LoadUint(SS_YAML_KEY_LAST3BYTES);
	*(UINT64*)m_VolTrkSecChk4and4 = yamlLoadHelper.LoadUint64(SS_YAML_KEY_VTSC_4AND4);
	m_4and4idx                    = yamlLoadHelper.LoadUint(SS_YAML_KEY_VTSC_4AND4_IDX);
	m_bAddressPrologueIsDOS3_2    = false;	// Doesn't matter if this is wrong - for DOS 3.2 INIT, will just get a long (non-truncated) track-0

	yamlLoadHelper.PopMap();
}
