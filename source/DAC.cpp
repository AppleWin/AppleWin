/*
   Emulate an 8 bit DAC (eg: SAM card) which writes unsigned byte
   data written to its IO area to the audio buffer (as used by the speaker).
   This merges the data with the speaker stream, reducing the volume
   of the Apple speaker when active.

   Riccardo Macri  Mar 2015
*/
#include "StdAfx.h"

#include "AppleWin.h"
#include "Memory.h"
#include "DAC.h"
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
   // regular speaker data still audible but muted until speaker
   // goes idle

   g_nSpeakerData = (d ^ 0x80) << 8;
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
