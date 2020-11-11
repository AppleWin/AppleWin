#pragma once

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

#include "DiskLog.h"

class FormatTrack	// Monitor for formatting of track
{
public:
	FormatTrack(bool bSuppressOutput=false)
	{
		m_bSuppressReadD5AAxxDetected = bSuppressOutput;
		Reset();
	};

	~FormatTrack(void) {};

	void Reset(void);
	void DriveNotWritingTrack(void);
	void DriveSwitchedToReadMode(class FloppyDisk* const pFloppy);
	void DriveSwitchedToWriteMode(UINT uTrackIndex);
	void DecodeLatchNibbleRead(BYTE floppylatch);
	void DecodeLatchNibbleWrite(BYTE floppylatch, UINT uSpinNibbleCount, const class FloppyDisk* const pFloppy, bool bIsSyncFF);
	std::string GetReadD5AAxxDetectedString(void) { std::string tmp = m_strReadD5AAxxDetected; m_strReadD5AAxxDetected = ""; return tmp; }
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

private:
	void UpdateOnWriteLatch(UINT uSpinNibbleCount, const class FloppyDisk* const pFloppy);
	void DecodeLatchNibble(BYTE floppylatch, bool bIsWrite, bool bIsSyncFF);

	BYTE m_VolTrkSecChk[4];

	UINT16 m_bmWrittenSectorAddrFields;
	UINT m_WriteTrackStartIndex;
	bool m_WriteTrackHasWrapped;
	BYTE m_WriteDataFieldPrologueCount;
	bool m_bAddressPrologueIsDOS3_2;

	enum TRACKSTATE_e {TS_GAP1, TS_ADDRFIELD, TS_GAP2_START, TS_GAP2, TS_DATAFIELD, TS_GAP3};	// Take care: value written to save-state
	TRACKSTATE_e m_trackState;
	UINT32 m_uLast3Bytes;
	BYTE m_VolTrkSecChk4and4[8];
	UINT m_4and4idx;

	std::string m_strReadD5AAxxDetected;
	bool m_bSuppressReadD5AAxxDetected;

#if LOG_DISK_NIBBLES_WRITE_TRACK_GAPS
	UINT m_DbgGap1Size;
	UINT m_DbgGap2Size;
	int m_DbgGap3Size;
#endif
};
