#pragma once

/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

#include "DiskImageHelper.h"

struct FluxTrack
{
	FluxTrack() :
		present(false),
		lapStart(0),
		lapEnd(0),
		loopTicks(0)
	{}

	bool present;
	UINT lapStart;		// offset into the image buffer of the loop's first byte
	UINT lapEnd;		// offset one past the loop's last byte (the seam)
	UINT64 loopTicks;	// ticks from lapStart to lapEnd: the angle carried across a seek is a fraction of this
	std::vector<UINT> checkpointBytes;		// seek checkpoints: the first byte of an interval...
	std::vector<UINT64> checkpointTicks;	// ...and the tick total before it
};

struct FluxImage
{
	bool IsFlux(const int quarterTrack) const { return quarterTrack >= 0 && quarterTrack < (int)CWOZHelper::MAX_QUARTER_TRACKS_5_25 && track[quarterTrack].present; }
	int NumFluxTracks() const;

	FluxTrack track[CWOZHelper::MAX_QUARTER_TRACKS_5_25];
};

// Returns "" when the FLUX chunk decoded, else the reason it is refused (fluxImage then holds a partial table)
// NB. A FLUX chunk that maps no quarter track gives no flux tracks (not an error)
std::string FluxParseWOZ(const BYTE* pImageBuffer, const UINT imageSize, const UINT fluxMapOffset, const UINT trksOffset, FluxImage& fluxImage);

//-------------------------------------

// The head on a flux position. Every time is in cycles from the start of the current pass of the loop.
class FluxCursor
{
public:
	FluxCursor()
	{
		clear();
	}

	void clear()
	{
		m_quarterTrack = -1;
		m_clock = 0.0;
		m_cyclesPerTick = 0.0;
		m_cellCycles = 0.0;
		m_nextByte = 0;
		m_ticks = 0;
		m_intervalByte = 0;
		m_intervalTicks = 0;
		m_now = 0.0;
		m_intervalStart = 0.0;
		m_intervalLength = 0.0;
		m_bitsInInterval = 0;
		m_bitsPlayed = 0;
	}

	void Land(const FluxImage& fluxImage, const BYTE* pImageBuffer, const int quarterTrack, const double fraction, const double clock);
	double GetAngle(const FluxImage& fluxImage) const;
	void AddTime(const FluxImage& fluxImage, const double cycles);
	bool GetNextBit(const FluxImage& fluxImage, const BYTE* pImageBuffer, BYTE& bit);
	void DropDue(const FluxImage& fluxImage, const BYTE* pImageBuffer);
	bool Restore(const FluxImage& fluxImage, const BYTE* pImageBuffer);

private:
	double GetBitTime(const UINT bit) const;
	void NextInterval(const FluxTrack& track, const BYTE* pImageBuffer);
	void SetClock(const double clock);
	void SeekTo(const FluxTrack& track, const BYTE* pImageBuffer, const double target);

public:
	int m_quarterTrack;			// -1 = the current track is not a flux position
	double m_clock;				// the 6502 base clock the cycle values were made with
	double m_cyclesPerTick;
	double m_cellCycles;		// 4us in cycles
	UINT m_nextByte;			// next byte to read
	UINT64 m_ticks;				// tick total from the loop start to the end of the current interval
	UINT m_intervalByte;		// first byte of the current interval
	UINT64 m_intervalTicks;		// tick total before the current interval
	double m_now;				// head time
	double m_intervalStart;		// current interval: start time, length, bits, bits already played
	double m_intervalLength;
	UINT m_bitsInInterval;
	UINT m_bitsPlayed;
};
