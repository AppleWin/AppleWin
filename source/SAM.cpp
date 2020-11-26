/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
  SAM.CPP

  Emulate an 8 bit DAC (eg: SAM card) which writes unsigned byte
  data written to its IO area to the audio buffer (as used by the speaker).
  This merges the data with the speaker stream, reducing the volume
  of the Apple speaker when active.

  Riccardo Macri  Mar 2015
*/
#include "StdAfx.h"

#include "SAM.h"
#include "Memory.h"
#include "Speaker.h"

//
// Write 8 bit data to speaker. Emulates a "SAM" speech card DAC
//


static BYTE __stdcall IOWrite_SAM(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	// Emulate audio from a SAM / 8 bit DAC card
	// Only supportable if AppleWin is using WAVE output
	//
	// This works by using the existing speaker handling but then 
	// replacing the speaker audio with the 8 bit samples from the DAC
	// before they get sent out to the soundcard buffer, whilst
	// audio samples are being written to the SAM.
	//
	// Whilst very unusual, it is possible to intermingle use of SAM and the apple
	// speaker. This is crudely supported with g_bQuieterSpeaker making the Apple
	// speaker produce quieter clicks which will be crudely intermingled
	// with the SAM data. The mute gets reset after the speaker code detects
	// silence.

	if (soundtype != SOUND_WAVE)
		return MemReadFloatingBus(nExecutedCycles);

	// use existing speaker code to bring timing up to date
	BYTE res = SpkrToggle(pc, addr, bWrite, d, nExecutedCycles);

	// The DAC in the SAM uses unsigned 8 bit samples
	// The WAV data that g_nSpeakerData is loaded into is a signed short
	//
	// We convert unsigned 8 bit to signed by toggling the most significant bit
	// 
	//  SAM card     WAV driver           SAM WAV
	//  0xFF 255     0x7f  127      _      FF  7F 
	//  0x81 129     0x01    1     / \     .
	//  0x80 128     0x00    0    /   \   /80  00
	//  0x7f 127     0xFF   -1         \_/
	//  0x00   0     0x80 -128             00  80
	//                                                        
	// SAM is 8 bit, PC WAV is 16 so shift audio to the MSB (<< 8)

	g_nSpeakerData = (d ^ 0x80) << 8;

	// make speaker quieter so eg: a metronome click through the
	// Apple speaker is softer vs. the analogue SAM output.
	g_bQuieterSpeaker = true;

	return res;
}

void ConfigureSAM(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	RegisterIoHandler(uSlot, IO_Null, IOWrite_SAM, IO_Null, IO_Null, NULL, NULL);
}
