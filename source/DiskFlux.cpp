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

/* Description: Timed flux player for WOZ 2.1 FLUX tracks (5.25" only, read only)
 *
 * Author: Various
 *
 * Ported from the Apple2TS implementation.
 */

#include "StdAfx.h"

#include "DiskFlux.h"
#include "StrFormat.h"

static const UINT FLUX_CHECKPOINT_BYTES = 64;
static const UINT WOZ_FLUX_PS_PER_TICK = 125000;	// WOZ 2.1: 125ns ticks
static const double FLUX_CELL_SECONDS = 4e-6;		// the drive's bit cell, in real time

int FluxImage::NumFluxTracks() const
{
	int numFluxTracks = 0;
	for (UINT i = 0; i < CWOZHelper::MAX_QUARTER_TRACKS_5_25; i++)
	{
		if (track[i].present)
			numFluxTracks++;
	}
	return numFluxTracks;
}

// The checkpoints of a loop: at every FLUX_CHECKPOINT_BYTES bytes, the first interval that starts at or past that byte,
// and the tick total before it.
// NB. A seek from a checkpoint gives the same cursor as the walk from the loop start.
static void BuildCheckpoints(FluxTrack& track, const BYTE* pImageBuffer)
{
	track.checkpointBytes.clear();
	track.checkpointTicks.clear();
	UINT nextCheckpoint = track.lapStart;
	UINT64 tickTotal = 0;
	UINT offset = track.lapStart;
	while (offset < track.lapEnd)
	{
		if (offset >= nextCheckpoint)
		{
			track.checkpointBytes.push_back(offset);
			track.checkpointTicks.push_back(tickTotal);
			while (nextCheckpoint <= offset)
				nextCheckpoint += FLUX_CHECKPOINT_BYTES;
		}

		BYTE ticks = 255;
		while (ticks == 255 && offset < track.lapEnd)
		{
			ticks = pImageBuffer[offset++];
			tickTotal += ticks;
		}
	}

	if (track.checkpointBytes.empty())
	{
		track.checkpointBytes.push_back(track.lapStart);
		track.checkpointTicks.push_back(0);
	}
}

static UINT64 GetLoopTicks(const BYTE* pImageBuffer, const UINT start, const UINT end)
{
	UINT64 ticks = 0;
	for (UINT i = start; i < end; i++)
		ticks += pImageBuffer[i];
	return ticks;
}

//===========================================================================

// WOZ 2.1: the FLUX chunk maps a quarter track to a TRKS entry whose data is flux: bytes of 125ns ticks, 255 = add and continue.
// The entry's bitCount is the byte count. FLUX wins over TMAP at that quarter track. The loop is the whole entry (one revolution).
std::string FluxParseWOZ(const BYTE* pImageBuffer, const UINT imageSize, const UINT fluxMapOffset, const UINT trksOffset, FluxImage& fluxImage)
{
	if ((UINT64)fluxMapOffset + CWOZHelper::MAX_QUARTER_TRACKS_5_25 > imageSize) return "the FLUX chunk runs past the end of the file";

	for (UINT quarterTrack = 0; quarterTrack < CWOZHelper::MAX_QUARTER_TRACKS_5_25; quarterTrack++)
	{
		fluxImage.track[quarterTrack] = FluxTrack();
		const BYTE index = pImageBuffer[fluxMapOffset + quarterTrack];
		if (index == CWOZHelper::TMAP_TRACK_EMPTY) continue;
		if (index >= CWOZHelper::MAX_QUARTER_TRACKS_5_25) return StrFormat("flux quarter track %u has a TRKS index past the table", quarterTrack);
		const UINT64 trkOffset = trksOffset + sizeof(CWOZHelper::TRKv2) * (UINT64)index;
		if (trkOffset + sizeof(CWOZHelper::TRKv2) > imageSize) return StrFormat("flux quarter track %u has a TRKS entry past the end of the file", quarterTrack);
		const CWOZHelper::TRKv2* pTRK = (CWOZHelper::TRKv2*) &pImageBuffer[trkOffset];
		const UINT64 start = CWOZHelper::BLOCK_SIZE * (UINT64)pTRK->startBlock;
		const UINT64 blockCount = pTRK->blockCount;
		const UINT64 byteCount = pTRK->bitCount;	// NB. for a flux track this is a byte count
		if (start == 0) return StrFormat("flux quarter track %u has no TRKS data", quarterTrack);
		if (byteCount == 0) return StrFormat("flux quarter track %u is empty", quarterTrack);
		if (start < 3 * CWOZHelper::BLOCK_SIZE || byteCount > CWOZHelper::BLOCK_SIZE * blockCount) return StrFormat("flux quarter track %u is not inside its TRKS blocks", quarterTrack);
		if (start + byteCount > imageSize) return StrFormat("flux quarter track %u runs past the end of the file", quarterTrack);
		if (pImageBuffer[start + byteCount - 1] == 255) return StrFormat("flux quarter track %u ends inside an interval", quarterTrack);

		FluxTrack& track = fluxImage.track[quarterTrack];
		track.lapStart = (UINT)start;
		track.lapEnd = (UINT)(start + byteCount);
		track.loopTicks = GetLoopTicks(pImageBuffer, track.lapStart, track.lapEnd);
		if (track.loopTicks == 0) return StrFormat("flux quarter track %u has no time in it", quarterTrack);
		track.present = true;
		BuildCheckpoints(track, pImageBuffer);
	}

	return "";
}

//===========================================================================

double FluxCursor::GetBitTime(const UINT bit) const
{
	return m_intervalStart + (double)bit * m_intervalLength / (double)m_bitsInInterval;
}

// Read the next interval. At the end of the loop, go back to its start with time continuous (the seam).
// NB. A flux position is one revolution: a loop of bytes [lapStart, lapEnd) in the image buffer. Each byte is a count of ticks;
// 255 means "add it and keep reading", a byte below 255 ends an interval with a flux transition.
// The time of a transition is the integer tick total since the loop start, converted to cycles in one step, so rounding
// never accumulates from one interval to the next. The cycle is the real 6502 clock (Get6502BaseClock), not the speed-slider clock.
void FluxCursor::NextInterval(const FluxTrack& track, const BYTE* pImageBuffer)
{
	if (m_nextByte >= track.lapEnd)
	{
		m_nextByte = track.lapStart;
		m_ticks = 0;
		m_now -= (double)track.loopTicks * m_cyclesPerTick;
	}

	m_intervalByte = m_nextByte;
	m_intervalTicks = m_ticks;
	BYTE ticks = 255;
	while (ticks == 255 && m_nextByte < track.lapEnd)
	{
		ticks = pImageBuffer[m_nextByte++];
		m_ticks += ticks;
	}

	m_intervalStart = (double)m_intervalTicks * m_cyclesPerTick;
	m_intervalLength = (double)m_ticks * m_cyclesPerTick - m_intervalStart;
	const double bits = floor(m_intervalLength / m_cellCycles + 0.5);
	m_bitsInInterval = (bits < 1) ? 1 : (UINT)bits;
	m_bitsPlayed = 0;
}

void FluxCursor::SetClock(const double clock)
{
	m_clock = clock;
	m_cyclesPerTick = (double)WOZ_FLUX_PS_PER_TICK * clock / 1e12;
	m_cellCycles = FLUX_CELL_SECONDS * clock;
}

// Put the head at 'target' cycles from the loop start (target < one loop)
// NB. The same cursor as the walk from the loop start, found from a checkpoint.
void FluxCursor::SeekTo(const FluxTrack& track, const BYTE* pImageBuffer, const double target)
{
	_ASSERT(!track.checkpointBytes.empty());

	// The last checkpoint at or before the target. Every interval before it ends at or before the target,
	// so the walk from the loop start would pass it with this very cursor.
	size_t first = 0;
	size_t last = track.checkpointBytes.size() - 1;
	while (first < last)
	{
		const size_t middle = (first + last + 1) >> 1;
		if ((double)track.checkpointTicks[middle] * m_cyclesPerTick <= target)
			first = middle;
		else
			last = middle - 1;
	}

	m_nextByte = track.checkpointBytes[first];
	m_ticks = track.checkpointTicks[first];
	m_now = 0.0;
	NextInterval(track, pImageBuffer);

	// Walk to the interval that holds the target. The target is inside the loop, so this ends before the seam.
	while ((double)m_ticks * m_cyclesPerTick <= target && m_nextByte < track.lapEnd)
		NextInterval(track, pImageBuffer);

	m_now = target;
	while (m_bitsPlayed < m_bitsInInterval - 1 && GetBitTime(m_bitsPlayed + 1) <= target)
		m_bitsPlayed++;
}

// Land on a quarter track at a fraction of one revolution
void FluxCursor::Land(const FluxImage& fluxImage, const BYTE* pImageBuffer, const int quarterTrack, const double fraction, const double clock)
{
	if (!fluxImage.IsFlux(quarterTrack))
	{
		clear();
		return;
	}

	const FluxTrack& track = fluxImage.track[quarterTrack];
	m_quarterTrack = quarterTrack;
	SetClock(clock);

	double landFraction = fraction;
	if (fraction < 0.0)
		landFraction = 0.0;
	else if (fraction > 1.0)
		landFraction = 1.0;

	SeekTo(track, pImageBuffer, landFraction * (double)track.loopTicks * m_cyclesPerTick);
}

// The head's angle on the cursor's position, as a fraction of one revolution
double FluxCursor::GetAngle(const FluxImage& fluxImage) const
{
	if (!fluxImage.IsFlux(m_quarterTrack))
		return 0.0;

	const double revolution = (double)fluxImage.track[m_quarterTrack].loopTicks * m_cyclesPerTick;
	if (revolution <= 0.0)
		return 0.0;

	double angle = fmod(m_now, revolution);
	if (angle < 0.0)
		angle += revolution;
	return angle / revolution;
}

// Turn the platter by 'cycles'. The bits that pass are then taken with GetNextBit() or DropDue().
void FluxCursor::AddTime(const FluxImage& fluxImage, const double cycles)
{
	if (!fluxImage.IsFlux(m_quarterTrack))
		return;
	if (m_bitsInInterval == 0)	// never landed
		return;
	if (cycles <= 0.0)	// time never runs backwards on the platter
		return;

	m_now += cycles;

	// A long idle gap: the track repeats every loop, so drop whole loops and play at most two
	const double loop = (double)fluxImage.track[m_quarterTrack].loopTicks * m_cyclesPerTick;
	const double behind = m_now - m_intervalStart;
	if (behind > 2 * loop)
		m_now -= (floor(behind / loop) - 1) * loop;
}

// The next bit that has passed the head, if any
// NB. An interval plays as max(1, round(interval / 4us)) bits, evenly spaced: all zeros, then a 1 at the transition.
// No synthetic bits are added: a long run without flux just gives many zeros.
bool FluxCursor::GetNextBit(const FluxImage& fluxImage, const BYTE* pImageBuffer, BYTE& bit)
{
	if (!fluxImage.IsFlux(m_quarterTrack) || m_bitsInInterval == 0)
		return false;
	if (GetBitTime(m_bitsPlayed + 1) > m_now)
		return false;

	m_bitsPlayed++;
	if (m_bitsPlayed == m_bitsInInterval)
	{
		bit = 1;
		NextInterval(fluxImage.track[m_quarterTrack], pImageBuffer);
	}
	else
	{
		bit = 0;
	}

	return true;
}

// Throw away every bit that has passed the head
void FluxCursor::DropDue(const FluxImage& fluxImage, const BYTE* pImageBuffer)
{
	if (!fluxImage.IsFlux(m_quarterTrack) || m_bitsInInterval == 0)
		return;

	const FluxTrack& track = fluxImage.track[m_quarterTrack];

	// A whole loop or more has passed (a long idle gap): seek from a checkpoint instead of walking.
	// NB. fmod is exact, as are the walk's seam subtractions, so the cursor is the one the walk gives.
	const double loop = (double)track.loopTicks * m_cyclesPerTick;
	if (m_now - m_intervalStart >= loop)
	{
		SeekTo(track, pImageBuffer, fmod(m_now, loop));
		return;
	}

	// Whole intervals first, then the bits of the interval that holds 'now'
	while (GetBitTime(m_bitsInInterval) <= m_now)
		NextInterval(track, pImageBuffer);
	while (m_bitsPlayed < m_bitsInInterval - 1 && GetBitTime(m_bitsPlayed + 1) <= m_now)
		m_bitsPlayed++;
}

// Pre: m_quarterTrack, m_clock, m_intervalByte, m_intervalTicks, m_bitsPlayed & m_now are loaded from a save-state
// Returns false if they do not fit the image
bool FluxCursor::Restore(const FluxImage& fluxImage, const BYTE* pImageBuffer)
{
	if (!fluxImage.IsFlux(m_quarterTrack))
		return false;

	const FluxTrack& track = fluxImage.track[m_quarterTrack];
	if (m_intervalByte < track.lapStart || m_intervalByte >= track.lapEnd || m_intervalTicks > track.loopTicks)
		return false;

	const UINT bitsPlayed = m_bitsPlayed;
	const double now = m_now;
	if (!(m_clock > 0.0))	// NB. also refuses a NaN
		return false;

	SetClock(m_clock);
	m_nextByte = m_intervalByte;
	m_ticks = m_intervalTicks;
	NextInterval(track, pImageBuffer);
	if (bitsPlayed >= m_bitsInInterval)
		return false;
	m_bitsPlayed = bitsPlayed;

	// The head must be in or just past this interval, as play leaves it; a corrupt time would
	// stall the player (far ahead) or silence it (behind). NB. The time is saved with 6 decimals.
	const double loop = (double)track.loopTicks * m_cyclesPerTick;
	if (!(now >= GetBitTime(bitsPlayed) - 1e-6 && now < m_intervalStart + m_intervalLength + loop))	// NB. also refuses a NaN
		return false;

	m_now = now;
	return true;
}
