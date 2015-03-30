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

#include "AppleWin.h"
#include "Memory.h"
#include "SAM.h"
#include "Speaker.h"

//
// Write 8 bit data to speaker. Emulates a "SAM" speech card DAC
//


static BYTE __stdcall IOWrite_DAC(WORD pc,  WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
 // Emulate audio from an 8 bit DAC
 // only if using wave data
 // uses the existing speaker for the timing but then
 // substitutes the analog data which gets subsequently
 // sent to audio out

 byte mb_res;

 if (soundtype == SOUND_WAVE)
  {
   // use existing speaker code to bring timing up to date
   mb_res = SpkrToggle(pc, addr, bWrite, d, nCyclesLeft);

   // 8 bit card      WAV driver (scaled by 256)
   //  0xFF 255       0x7f  127
   //  0x80 128       0x00    0
   //  0x7f 127       0xFF   -1
   //  0x00  0        0x80 -128
   
   // replace regular speaker output with 8 bit DAC value
   // regular speaker data still audible but quieter until speaker
   // goes idle (so both work simultaneously)

   g_nSpeakerData = (d ^ 0x80) << 8;

   // make speaker quieter so eg: a metronome click through the
   // Apple speaker is softer vs. the analogue SAM output
   g_quieterSpeaker = 1;
  }
 else
  mb_res = MemReadFloatingBus(nCyclesLeft);

 return mb_res;
}

void ConfigureDAC(LPBYTE pCxRomPeripheral, UINT uSlot)
{	
 RegisterIoHandler(uSlot,IO_Null,IOWrite_DAC,IO_Null,IO_Null, NULL, NULL);
}
