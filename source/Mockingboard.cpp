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

/* Description: Mockingboard/Phasor emulation
 *
 * Author: Copyright (c) 2002-2006, Tom Charlesworth
 */

// History:
//
// v1.12.07.1 (30 Dec 2005)
// - Update 6522 TIMERs after every 6502 opcode, giving more precise IRQs
// - Minimum TIMER freq is now 0x100 cycles
// - Added Phasor support
//
// v1.12.06.1 (16 July 2005)
// - Reworked 6522's ORB -> AY8910 decoder
// - Changed MB output so L=All voices from AY0 & AY2 & R=All voices from AY1 & AY3
// - Added crude support for Votrax speech chip (by using SSI263 phonemes)
//
// v1.12.04.1 (14 Sep 2004)
// - Switch MB output from dual-mono to stereo.
// - Relaxed TIMER1 freq from ~62Hz (period=0x4000) to ~83Hz (period=0x3000).
//
// 25 Apr 2004:
// - Added basic support for the SSI263 speech chip
//
// 15 Mar 2004:
// - Switched to MAME's AY8910 emulation (includes envelope support)
//
// v1.12.03 (11 Jan 2004)
// - For free-running 6522 timer1 IRQ, reload with current ACCESS_TIMER1 value.
//   (Fixes Ultima 4/5 playback speed problem.)
//
// v1.12.01 (24 Nov 2002)
// - Shaped the tone waveform more logarithmically
// - Added support for MB ena/dis switch on Config dialog
// - Added log file support
//
// v1.12.00 (17 Nov 2002)
// - Initial version (no AY8910 envelope support)
//

// Notes on Votrax chip (on original Mockingboards):
// From Crimewave (Penguin Software):
// . Init:
//   . DDRB = 0xFF
//   . PCR  = 0xB0
//   . IER  = 0x90
//   . ORB  = 0x03 (PAUSE0) or 0x3F (STOP)
// . IRQ:
//   . ORB  = Phoneme value
// . IRQ last phoneme complete:
//   . IER  = 0x10
//   . ORB  = 0x3F (STOP)
//

#include "StdAfx.h"

#include "Mockingboard.h"
#include "SaveState_Structs_v1.h"

#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Log.h"
#include "Memory.h"
#include "SoundCore.h"
#include "SynchronousEventManager.h"
#include "YamlHelper.h"
#include "Riff.h"

#include "AY8910.h"
#include "SSI263Phonemes.h"

#define LOG_SSI263 0
#define LOG_SSI263B 0	// Alternate SSI263 logging (use in conjunction with CPU.cpp's LOG_IRQ_TAKEN_AND_RTI)


#define SY6522_DEVICE_A 0
#define SY6522_DEVICE_B 1

#define SLOT4 4
#define SLOT5 5

#define NUM_MB 2
#define NUM_DEVS_PER_MB 2
#define NUM_AY8910 (NUM_MB*NUM_DEVS_PER_MB)
#define NUM_SY6522 NUM_AY8910
#define NUM_VOICES_PER_AY8910 3
#define NUM_VOICES (NUM_AY8910*NUM_VOICES_PER_AY8910)


// Chip offsets from card base.
#define SY6522A_Offset	0x00
#define SY6522B_Offset	0x80
#define SSI263_Offset	0x40

//#define Phasor_SY6522A_CS		4
//#define Phasor_SY6522B_CS		7
//#define Phasor_SY6522A_Offset	(1<<Phasor_SY6522A_CS)
//#define Phasor_SY6522B_Offset	(1<<Phasor_SY6522B_CS)

enum MockingboardUnitState_e {AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH};

struct SY6522_AY8910
{
	SY6522 sy6522;
	BYTE nAY8910Number;
	BYTE nAYCurrentRegister;
	bool bTimer1Active;
	bool bTimer2Active;
	SSI263A SpeechChip;
	MockingboardUnitState_e state;	// Where a unit is a 6522+AY8910 pair
	MockingboardUnitState_e stateB;	// Phasor: 6522 & 2nd AY8910
};


// IFR & IER:
#define IxR_PERIPHERAL	(1<<1)
#define IxR_VOTRAX		(1<<4)	// TO DO: Get proper name from 6522 datasheet!
#define IxR_TIMER2		(1<<5)
#define IxR_TIMER1		(1<<6)

// ACR:
#define RUNMODE		(1<<6)	// 0 = 1-Shot Mode, 1 = Free Running Mode
#define RM_ONESHOT		(0<<6)
#define RM_FREERUNNING	(1<<6)


// SSI263A registers:
#define SSI_DURPHON	0x00
#define SSI_INFLECT	0x01
#define SSI_RATEINF	0x02
#define SSI_CTTRAMP	0x03
#define SSI_FILFREQ	0x04


// Support 2 MB's, each with 2x SY6522/AY8910 pairs.
static SY6522_AY8910 g_MB[NUM_AY8910];

const UINT kExtraTimerCycles = 2;	// Rockwell, Fig.16: period = N+2 cycles
const UINT kNumTimersPer6522 = 2;
const UINT kNumSyncEvents = NUM_MB * NUM_SY6522 * kNumTimersPer6522;
static SyncEvent* g_syncEvent[kNumSyncEvents];

// Timer vars
static const UINT kTIMERDEVICE_INVALID = -1;
static UINT g_nMBTimerDevice = kTIMERDEVICE_INVALID;	// SY6522 device# which is generating timer IRQ
static UINT64 g_uLastCumulativeCycles = 0;

// SSI263 vars:
static USHORT g_nSSI263Device = 0;	// SSI263 device# which is generating phoneme-complete IRQ
static volatile int g_nCurrentActivePhoneme = -1;	// Modified by threads: main & SSI263Thread
static volatile bool g_bStopPhoneme = false;		// Modified by threads: main & SSI263Thread
static bool g_bVotraxPhoneme = false;

static const DWORD SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample

static short* ppAYVoiceBuffer[NUM_VOICES] = {0};

static unsigned __int64	g_nMB_InActiveCycleCount = 0;
static bool g_bMB_RegAccessedFlag = false;
static bool g_bMB_Active = false;

static HANDLE g_hThread = NULL;

static bool g_bMBAvailable = false;

//

static SS_CARDTYPE g_SoundcardType = CT_Empty;	// Use CT_Empty to mean: no soundcard
static bool g_bPhasorEnable = false;
enum PHASOR_MODE {PH_Mockingboard=0, PH_UNDEF1, PH_UNDEF2, PH_UNDEF3, PH_UNDEF4, PH_Phasor/*=5*/, PH_UNDEF6, PH_EchoPlus/*=7*/};
static PHASOR_MODE g_phasorMode = PH_Mockingboard;
static UINT g_PhasorClockScaleFactor = 1;	// for save-state only

//-------------------------------------

static const unsigned short g_nMB_NumChannels = 2;

static const DWORD g_dwDSBufferSize = MAX_SAMPLES * sizeof(short) * g_nMB_NumChannels;

static const SHORT nWaveDataMin = (SHORT)0x8000;
static const SHORT nWaveDataMax = (SHORT)0x7FFF;

static short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];


static VOICE MockingboardVoice = {0};
static VOICE SSI263Voice[64] = {0};

static const int g_nNumEvents = 2;
static HANDLE g_hSSI263Event[g_nNumEvents] = {NULL};	// 1: Phoneme finished playing, 2: Exit thread
static DWORD g_dwMaxPhonemeLen = 0;

static bool g_bCritSectionValid = false;	// Deleting CritialSection when not valid causes crash on Win98
static CRITICAL_SECTION g_CriticalSection;	// To guard 6522's IFR

static UINT g_cyclesThisAudioFrame = 0;

//---------------------------------------------------------------------------

// Forward refs:
static DWORD WINAPI SSI263Thread(LPVOID);
static void Votrax_Write(BYTE nDevice, BYTE nValue);
static int MB_SyncEventCallback(int id, int cycles, ULONG uExecutedCycles);

//---------------------------------------------------------------------------

static void StartTimer1(SY6522_AY8910* pMB)
{
	pMB->bTimer1Active = true;

	if (pMB->sy6522.IER & IxR_TIMER1)			// Using 6522 interrupt
		g_nMBTimerDevice = pMB->nAY8910Number;
	else if (pMB->sy6522.ACR & RM_FREERUNNING)	// Polling 6522 IFR (GH#496)
		g_nMBTimerDevice = pMB->nAY8910Number;
}

// The assumption was that timer1 was only active if IER.TIMER1=1
// . Not true, since IFR can be polled (with IER.TIMER1=0)
static void StartTimer1_LoadStateV1(SY6522_AY8910* pMB)
{
	if ((pMB->sy6522.IER & IxR_TIMER1) == 0x00)
		return;

	pMB->bTimer1Active = true;
	g_nMBTimerDevice = pMB->nAY8910Number;
}

static void StopTimer1(SY6522_AY8910* pMB)
{
	pMB->bTimer1Active = false;
	g_nMBTimerDevice = kTIMERDEVICE_INVALID;
}

//-----------------------------------------------------------------------------

static void StartTimer2(SY6522_AY8910* pMB)
{
	pMB->bTimer2Active = true;

	// NB. Can't mimic StartTimer1() as that would stomp on global state
	// TODO: Switch to per-device state
}

static void StopTimer2(SY6522_AY8910* pMB)
{
	pMB->bTimer2Active = false;
}

//-----------------------------------------------------------------------------

static void ResetSY6522(SY6522_AY8910* pMB)
{
	memset(&pMB->sy6522,0,sizeof(SY6522));

	StopTimer1(pMB);
	StopTimer2(pMB);

	pMB->nAYCurrentRegister = 0;
	pMB->state = AY_INACTIVE;
	pMB->stateB = AY_INACTIVE;
}

//-----------------------------------------------------------------------------

static void AY8910_Write(BYTE nDevice, BYTE /*nReg*/, BYTE nValue, BYTE nAYDevice)
{
	g_bMB_RegAccessedFlag = true;
	SY6522_AY8910* pMB = &g_MB[nDevice];

	if ((nValue & 4) == 0)
	{
		// RESET: Reset AY8910 only
		AY8910_reset(nDevice+2*nAYDevice);
	}
	else
	{
		// Determine the AY8910 inputs
		int nBDIR = (nValue & 2) ? 1 : 0;
		const int nBC2 = 1;		// Hardwired to +5V
		int nBC1 = nValue & 1;

		MockingboardUnitState_e nAYFunc = (MockingboardUnitState_e) ((nBDIR<<2) | (nBC2<<1) | nBC1);
		MockingboardUnitState_e& state = (nAYDevice == 0) ? pMB->state : pMB->stateB;	// GH#659

#if _DEBUG
		if (!g_bPhasorEnable)
			_ASSERT(nAYDevice == 0);
		if (nAYFunc == AY_WRITE || nAYFunc == AY_LATCH)
			_ASSERT(state == AY_INACTIVE);
#endif

		if (state == AY_INACTIVE)	// GH#320: functions only work from inactive state
		{
			switch (nAYFunc)
			{
				case AY_INACTIVE:	// 4: INACTIVE
					break;

				case AY_READ:		// 5: READ FROM PSG (need to set DDRA to input)
					break;

				case AY_WRITE:		// 6: WRITE TO PSG
					_AYWriteReg(nDevice+2*nAYDevice, pMB->nAYCurrentRegister, pMB->sy6522.ORA);
					break;

				case AY_LATCH:		// 7: LATCH ADDRESS
					// http://www.worldofspectrum.org/forums/showthread.php?t=23327
					// Selecting an unused register number above 0x0f puts the AY into a state where
					// any values written to the data/address bus are ignored, but can be read back
					// within a few tens of thousands of cycles before they decay to zero.
					if(pMB->sy6522.ORA <= 0x0F)
						pMB->nAYCurrentRegister = pMB->sy6522.ORA & 0x0F;
					// else Pro-Mockingboard (clone from HK)
					break;
			}
		}

		state = nAYFunc;
	}
}

static UINT GetOpcodeCycles(BYTE reg)
{
	UINT opcodeCycles = 0;
	BYTE opcode = 0;
	bool abs16 = false;

	const BYTE opcodeMinus3 = mem[(regs.pc-3)&0xffff];
	const BYTE opcodeMinus2 = mem[(regs.pc-2)&0xffff];

	if ( (opcodeMinus3 == 0x8C) ||		// sty abs16
		 (opcodeMinus3 == 0x8D) ||		// sta abs16
		 (opcodeMinus3 == 0x8E) )		// stx abs16
	{	// Eg. FT demos: CHIP, MADEF, MAD2
		opcodeCycles = 4;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else if ( (opcodeMinus3 == 0x99) ||	// sta abs16,y
			  (opcodeMinus3 == 0x9D) )	// sta abs16,x
	{	// Eg. Paleotronic microTracker demo
		opcodeCycles = 5;
		opcode = opcodeMinus3;
		abs16 = true;
	}
	else if (opcodeMinus2 == 0x81)		// sta (zp,x)
	{
		opcodeCycles = 6;
		opcode = opcodeMinus2;
	}
	else if (opcodeMinus2 == 0x91)		// sta (zp),y
	{	// Eg. FT demos: OMT, PLS
		opcodeCycles = 6;
		opcode = opcodeMinus2;
	}
	else if (opcodeMinus2 == 0x92 && GetMainCpu() == CPU_65C02)		// sta (zp) : 65C02-only
	{
		opcodeCycles = 5;
		opcode = opcodeMinus2;
	}
	else
	{
		_ASSERT(0);
		opcodeCycles = 0;
		return 0;
	}

	//

	WORD addr16 = 0;

	if (!abs16)
	{
		BYTE zp = mem[(regs.pc-1)&0xffff];
		if (opcode == 0x81) zp += regs.x;
		addr16 = (mem[zp] | (mem[(zp+1)&0xff]<<8));
		if (opcode == 0x91) addr16 += regs.y;
	}
	else
	{
		addr16 = mem[(regs.pc-2)&0xffff] | (mem[(regs.pc-1)&0xffff]<<8);
		if (opcode == 0x99) addr16 += regs.y;
		if (opcode == 0x9D) addr16 += regs.x;
	}

	// Check we've reverse looked-up the 6502 opcode correctly
	if ((addr16 & 0xF80F) != (0xC000+reg))
	{
		_ASSERT(0);
		return 0;
	}

	return opcodeCycles;
}

// Insert a new synchronous event whenever the 6522 timer's counter is written.
// . NB. it doesn't matter if the timer's interrupt enable (IER) is set or not
//   - the state of IER is only important when the counter underflows - see: MB_SyncEventCallback()
static USHORT SetTimerSyncEvent(UINT id, BYTE reg, USHORT timerLatch)
{
	// NB. This TIMER adjustment value gets subtracted when this current opcode completes, so no need to persist to save-state
	const UINT opcodeCycleAdjust = GetOpcodeCycles(reg);

	SyncEvent* pSyncEvent = g_syncEvent[id];
	if (pSyncEvent->m_active)
		g_SynchronousEventMgr.Remove(id);

	pSyncEvent->SetCycles(timerLatch + kExtraTimerCycles + opcodeCycleAdjust);
	g_SynchronousEventMgr.Insert(pSyncEvent);

	// It doesn't matter if this overflows (ie. >0xFFFF), since on completion of current opcode it'll be corrected
	return (USHORT) (timerLatch + opcodeCycleAdjust);
}

static void UpdateIFR(SY6522_AY8910* pMB, BYTE clr_ifr, BYTE set_ifr=0)
{
	// Need critical section to avoid data-race: main thread & SSI263Thread can both access IFR
	// . NB. Loading a save-state just directly writes into 6522.IFR (which is fine)
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	{
		pMB->sy6522.IFR &= ~clr_ifr;
		pMB->sy6522.IFR |= set_ifr;

		if (pMB->sy6522.IFR & pMB->sy6522.IER & 0x7F)
			pMB->sy6522.IFR |= 0x80;
		else
			pMB->sy6522.IFR &= 0x7F;
	}
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);

	// Now update the IRQ signal from all 6522s
	// . OR-sum of all active TIMER1, TIMER2 & SPEECH sources (from all 6522s)
	UINT bIRQ = 0;
	for (UINT i=0; i<NUM_SY6522; i++)
		bIRQ |= g_MB[i].sy6522.IFR & 0x80;

	// NB. Mockingboard generates IRQ on both 6522s:
	// . SSI263's IRQ (A/!R) is routed via the 2nd 6522 (at $Cx80) and must generate a 6502 IRQ (not NMI)
	// . SC-01's IRQ (A/!R) is also routed via a (2nd?) 6522
	// Phasor's SSI263 IRQ (A/!R) line is *also* wired directly to the 6502's IRQ (as well as the 6522's CA1)

	if (bIRQ)
	    CpuIrqAssert(IS_6522);
	else
	    CpuIrqDeassert(IS_6522);
}

static void SY6522_Write(BYTE nDevice, BYTE nReg, BYTE nValue)
{
	g_bMB_Active = true;

	SY6522_AY8910* pMB = &g_MB[nDevice];

	switch (nReg)
	{
		case 0x00:	// ORB
			{
				nValue &= pMB->sy6522.DDRB;
				pMB->sy6522.ORB = nValue;

				if( (pMB->sy6522.DDRB == 0xFF) && (pMB->sy6522.PCR == 0xB0) )
				{
					// Votrax speech data
					Votrax_Write(nDevice, nValue);
					break;
				}

				if(g_bPhasorEnable)
				{
					int nAY_CS = (g_phasorMode == PH_Phasor) ? (~(nValue >> 3) & 3) : 1;

					if(nAY_CS & 1)
						AY8910_Write(nDevice, nReg, nValue, 0);

					if(nAY_CS & 2)
						AY8910_Write(nDevice, nReg, nValue, 1);
				}
				else
				{
					AY8910_Write(nDevice, nReg, nValue, 0);
				}

				break;
			}
		case 0x01:	// ORA
			pMB->sy6522.ORA = nValue & pMB->sy6522.DDRA;
			break;
		case 0x02:	// DDRB
			pMB->sy6522.DDRB = nValue;
			break;
		case 0x03:	// DDRA
			pMB->sy6522.DDRA = nValue;
			break;
		case 0x04:	// TIMER1L_COUNTER
		case 0x06:	// TIMER1L_LATCH
			pMB->sy6522.TIMER1_LATCH.l = nValue;
			break;
		case 0x05:	// TIMER1H_COUNTER
			{
				UpdateIFR(pMB, IxR_TIMER1);			// Clear Timer1 Interrupt Flag
				pMB->sy6522.TIMER1_LATCH.h = nValue;
				const UINT id = nDevice*kNumTimersPer6522+0;	// TIMER1
				pMB->sy6522.TIMER1_COUNTER.w = SetTimerSyncEvent(id, nReg, pMB->sy6522.TIMER1_LATCH.w);
				StartTimer1(pMB);
			}
			break;
		case 0x07:	// TIMER1H_LATCH
			UpdateIFR(pMB, IxR_TIMER1);				// Clear Timer1 Interrupt Flag
			pMB->sy6522.TIMER1_LATCH.h = nValue;
			break;
		case 0x08:	// TIMER2L
			pMB->sy6522.TIMER2_LATCH.l = nValue;
			break;
		case 0x09:	// TIMER2H
			{
				UpdateIFR(pMB, IxR_TIMER2);			// Clear Timer2 Interrupt Flag
				pMB->sy6522.TIMER2_LATCH.h = nValue;	// NB. Real 6522 doesn't have TIMER2_LATCH.h
				const UINT id = nDevice*kNumTimersPer6522+1;	// TIMER2
				pMB->sy6522.TIMER2_COUNTER.w = SetTimerSyncEvent(id, nReg, pMB->sy6522.TIMER2_LATCH.w);
				StartTimer2(pMB);
			}
			break;
		case 0x0a:	// SERIAL_SHIFT
			break;
		case 0x0b:	// ACR
			pMB->sy6522.ACR = nValue;
			break;
		case 0x0c:	// PCR -  Used for Speech chip only
			pMB->sy6522.PCR = nValue;
			break;
		case 0x0d:	// IFR
			// - Clear those bits which are set in the lower 7 bits.
			// - Can't clear bit 7 directly.
			UpdateIFR(pMB, nValue);
			break;
		case 0x0e:	// IER
			if(!(nValue & 0x80))
			{
				// Clear those bits which are set in the lower 7 bits.
				nValue ^= 0x7F;
				pMB->sy6522.IER &= nValue;
			}
			else
			{
				// Set those bits which are set in the lower 7 bits.
				nValue &= 0x7F;
				pMB->sy6522.IER |= nValue;
			}
			UpdateIFR(pMB, 0);
			break;
		case 0x0f:	// ORA_NO_HS
			break;
	}
}

//-----------------------------------------------------------------------------

static BYTE SY6522_Read(BYTE nDevice, BYTE nReg)
{
//	g_bMB_RegAccessedFlag = true;
	g_bMB_Active = true;

	SY6522_AY8910* pMB = &g_MB[nDevice];
	BYTE nValue = 0x00;

	switch (nReg)
	{
		case 0x00:	// ORB
			nValue = pMB->sy6522.ORB;
			break;
		case 0x01:	// ORA
			nValue = pMB->sy6522.ORA;
			break;
		case 0x02:	// DDRB
			nValue = pMB->sy6522.DDRB;
			break;
		case 0x03:	// DDRA
			nValue = pMB->sy6522.DDRA;
			break;
		case 0x04:	// TIMER1L_COUNTER
			// NB. GH#701 (T1C:=0xFFFF, LDA T1C_L, A==0xFC)
			nValue = (pMB->sy6522.TIMER1_COUNTER.w - 3) & 0xff;		// -3 to compensate for the (assumed) 4-cycle STA 6522.T1C_H
			UpdateIFR(pMB, IxR_TIMER1);
			break;
		case 0x05:	// TIMER1H_COUNTER
			nValue = pMB->sy6522.TIMER1_COUNTER.h;
			break;
		case 0x06:	// TIMER1L_LATCH
			nValue = pMB->sy6522.TIMER1_LATCH.l;
			break;
		case 0x07:	// TIMER1H_LATCH
			nValue = pMB->sy6522.TIMER1_LATCH.h;
			break;
		case 0x08:	// TIMER2L
			nValue = pMB->sy6522.TIMER2_COUNTER.l;
			UpdateIFR(pMB, IxR_TIMER2);
			break;
		case 0x09:	// TIMER2H
			nValue = pMB->sy6522.TIMER2_COUNTER.h;
			break;
		case 0x0a:	// SERIAL_SHIFT
			break;
		case 0x0b:	// ACR
			nValue = pMB->sy6522.ACR;
			break;
		case 0x0c:	// PCR
			nValue = pMB->sy6522.PCR;
			break;
		case 0x0d:	// IFR
			nValue = pMB->sy6522.IFR;
			break;
		case 0x0e:	// IER
			nValue = 0x80 | pMB->sy6522.IER;	// GH#567
			break;
		case 0x0f:	// ORA_NO_HS
			nValue = pMB->sy6522.ORA;
			break;
	}

	return nValue;
}

//---------------------------------------------------------------------------

static void SSI263_Play(unsigned int nPhoneme);

#if 0
typedef struct
{
	BYTE DurationPhoneme;
	BYTE Inflection;		// I10..I3
	BYTE RateInflection;
	BYTE CtrlArtAmp;
	BYTE FilterFreq;
	//
	BYTE CurrentMode;
} SSI263A;
#endif

//static SSI263A nSpeechChip;

// Duration/Phonome
const BYTE DURATION_MODE_MASK = 0xC0;
const BYTE PHONEME_MASK = 0x3F;

const BYTE MODE_PHONEME_TRANSITIONED_INFLECTION = 0xC0;	// IRQ active
const BYTE MODE_PHONEME_IMMEDIATE_INFLECTION = 0x80;	// IRQ active
const BYTE MODE_FRAME_IMMEDIATE_INFLECTION = 0x40;		// IRQ active
const BYTE MODE_IRQ_DISABLED = 0x00;

// Rate/Inflection
const BYTE RATE_MASK = 0xF0;
const BYTE INFLECTION_MASK_H = 0x08;	// I11
const BYTE INFLECTION_MASK_L = 0x07;	// I2..I0

// Ctrl/Art/Amp
const BYTE CONTROL_MASK = 0x80;
const BYTE ARTICULATION_MASK = 0x70;
const BYTE AMPLITUDE_MASK = 0x0F;

#if LOG_SSI263B
static int ssiRegs[5]={-1,-1,-1,-1,-1};

void SSI_Output(void)
{
	LogOutput("SSI: ");
	for (int i=0; i<=4; i++)
	{
		char r[3]="--";
		if (ssiRegs[i]>=0) sprintf(r,"%02X",ssiRegs[i]);
		LogOutput("%s ", r);
		ssiRegs[i] = -1;
	}
	LogOutput("\n");
}
#endif

static BYTE SSI263_Read(BYTE nDevice, ULONG nExecutedCycles)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	// Regardless of register, just return inverted A/!R in bit7
	// . A/!R is low for IRQ

	return MemReadFloatingBus(pMB->SpeechChip.CurrentMode & 1, nExecutedCycles);
}

static void SSI263_Write(BYTE nDevice, BYTE nReg, BYTE nValue)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

#if LOG_SSI263B
	_ASSERT(nReg < 5);
	if (nReg>4) nReg=4;
	if (ssiRegs[nReg]>=0) SSI_Output();	// overwriting a reg
	ssiRegs[nReg] = nValue;
#endif

	switch(nReg)
	{
	case SSI_DURPHON:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "DUR   = 0x%02X, PHON = 0x%02X\n\n", nValue>>6, nValue&PHONEME_MASK);
		LogOutput("DUR   = %d, PHON = 0x%02X\n", nValue>>6, nValue&PHONEME_MASK);
#endif
#if LOG_SSI263B
		SSI_Output();
#endif

		// Notes:
		// . Phasor's text-to-speech playback has no CTL H->L
		//		- ISR just writes CTL=0 (and new ART+AMP values), and writes DUR=x (and new PHON)
		//		- since no CTL H->L, then DUR value doesn't take affect (so continue using previous)
		//		- so the write to DURPHON must clear the IRQ
		// . Does a write of CTL=0 clear IRQ? (ie. CTL 0->0)
		// . Does a write of CTL=1 clear IRQ? (ie. CTL 0->1)
		//		- SSI263 datasheet says: "Setting the Control bit (CTL) to a logic one puts the device into Power Down mode..."
		// . Does phoneme output only happen when CTL=0? (Otherwise device is in PD mode)

		// SSI263 datasheet is not clear, but a write to DURPHON must clear the IRQ.
		// NB. For Mockingboard, A/!R is ack'ed by 6522's PCR handshake.
		if (g_bPhasorEnable && g_phasorMode == PH_Phasor)
			CpuIrqDeassert(IS_SPEECH);

		pMB->SpeechChip.CurrentMode &= ~1;	// Clear SSI263's D7 pin

		pMB->SpeechChip.DurationPhoneme = nValue;

		g_nSSI263Device = nDevice;

		SSI263_Play(nValue & PHONEME_MASK);
		break;
	case SSI_INFLECT:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "INF   = 0x%02X\n", nValue);
#endif
		pMB->SpeechChip.Inflection = nValue;
		break;

	case SSI_RATEINF:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "RATE  = 0x%02X, INF = 0x%02X\n", nValue>>4, nValue&0x0F);
#endif
		pMB->SpeechChip.RateInflection = nValue;
		break;
	case SSI_CTTRAMP:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "CTRL  = %d, ART = 0x%02X, AMP=0x%02X\n", nValue>>7, (nValue&ARTICULATION_MASK)>>4, nValue&AMPLITUDE_MASK);
		//
		{
			bool H2L = (pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK);
			char newMode[20];
			sprintf_s(newMode, sizeof(newMode), "(new mode=%d)", pMB->SpeechChip.DurationPhoneme>>6);
			LogOutput("CTRL  = %d->%d, ART = 0x%02X, AMP=0x%02X %s\n", pMB->SpeechChip.CtrlArtAmp>>7, nValue>>7, (nValue&ARTICULATION_MASK)>>4, nValue&AMPLITUDE_MASK, H2L?newMode:"");
		}
#endif
#if LOG_SSI263B
		if ( ((pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK)) || ((nValue&0xF) == 0x0) )	// H->L or amp=0
			SSI_Output();
#endif
		if((pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK))	// H->L
		{
			pMB->SpeechChip.CurrentMode = pMB->SpeechChip.DurationPhoneme & DURATION_MODE_MASK;
			if (pMB->SpeechChip.CurrentMode == MODE_IRQ_DISABLED)
			{
				// "Disables A/!R output only; does not change previous A/!R response" (SSI263 datasheet)
//				CpuIrqDeassert(IS_SPEECH);
			}
		}

		pMB->SpeechChip.CtrlArtAmp = nValue;

		// "Setting the Control bit (CTL) to a logic one puts the device into Power Down mode..." (SSI263 datasheet)
		if (pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK)
		{
//			CpuIrqDeassert(IS_SPEECH);
//			pMB->SpeechChip.CurrentMode &= ~1;	// Clear SSI263's D7 pin
		}
		break;
	case SSI_FILFREQ:	// RegAddr.b2=1 (b1 & b0 are: don't care)
	default:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "FFREQ = 0x%02X\n", nValue);
#endif
		pMB->SpeechChip.FilterFreq = nValue;
		break;
	}
}

//-------------------------------------

static BYTE Votrax2SSI263[64] = 
{
	0x02,	// 00: EH3 jackEt -> E1 bEnt
	0x0A,	// 01: EH2 Enlist -> EH nEst
	0x0B,	// 02: EH1 hEAvy -> EH1 bElt
	0x00,	// 03: PA0 no sound -> PA
	0x28,	// 04: DT buTTer -> T Tart
	0x08,	// 05: A2 mAde -> A mAde
	0x08,	// 06: A1 mAde -> A mAde
	0x2F,	// 07: ZH aZure -> Z Zero
	0x0E,	// 08: AH2 hOnest -> AH gOt
	0x07,	// 09: I3 inhibIt -> I sIx
	0x07,	// 0A: I2 Inhibit -> I sIx
	0x07,	// 0B: I1 inhIbit -> I sIx
	0x37,	// 0C: M Mat -> More
	0x38,	// 0D: N suN -> N NiNe
	0x24,	// 0E: B Bag -> B Bag
	0x33,	// 0F: V Van -> V Very
	//
	0x32,	// 10: CH* CHip -> SCH SHip (!)
	0x32,	// 11: SH SHop ->  SCH SHip
	0x2F,	// 12: Z Zoo -> Z Zero
	0x10,	// 13: AW1 lAWful -> AW Office
	0x39,	// 14: NG thiNG -> NG raNG
	0x0F,	// 15: AH1 fAther -> AH1 fAther
	0x13,	// 16: OO1 lOOking -> OO lOOk
	0x13,	// 17: OO bOOK -> OO lOOk
	0x20,	// 18: L Land -> L Lift
	0x29,	// 19: K triCK -> Kit
	0x25,	// 1A: J* juDGe -> D paiD (!)
	0x2C,	// 1B: H Hello -> HF Heart
	0x26,	// 1C: G Get -> KV taG
	0x34,	// 1D: F Fast -> F Four
	0x25,	// 1E: D paiD -> D paiD
	0x30,	// 1F: S paSS -> S Same
	//
	0x08,	// 20: A dAY -> A mAde
	0x09,	// 21: AY dAY -> AI cAre
	0x03,	// 22: Y1 Yard -> YI Year
	0x1B,	// 23: UH3 missIOn -> UH3 nUt
	0x0E,	// 24: AH mOp -> AH gOt
	0x27,	// 25: P Past -> P Pen
	0x11,	// 26: O cOld -> O stOre
	0x07,	// 27: I pIn -> I sIx
	0x16,	// 28: U mOve -> U tUne
	0x05,	// 29: Y anY -> AY plEAse
	0x28,	// 2A: T Tap -> T Tart
	0x1D,	// 2B: R Red -> R Roof
	0x01,	// 2C: E mEEt -> E mEEt
	0x23,	// 2D: W Win -> W Water
	0x0C,	// 2E: AE dAd -> AE dAd
	0x0D,	// 2F: AE1 After -> AE1 After
	//
	0x10,	// 30: AW2 sAlty -> AW Office
	0x1A,	// 31: UH2 About -> UH2 whAt
	0x19,	// 32: UH1 Uncle -> UH1 lOve
	0x18,	// 33: UH cUp -> UH wOnder
	0x11,	// 34: O2 fOr -> O stOre
	0x11,	// 35: O1 abOArd -> O stOre
	0x14,	// 36: IU yOU -> IU yOU
	0x14,	// 37: U1 yOU -> IU yOU
	0x35,	// 38: THV THe -> THV THere
	0x36,	// 39: TH THin -> TH wiTH
	0x1C,	// 3A: ER bIrd -> ER bIrd
	0x0A,	// 3B: EH gEt -> EH nEst
	0x01,	// 3C: E1 bE -> E mEEt
	0x10,	// 3D: AW cAll -> AW Office
	0x00,	// 3E: PA1 no sound -> PA
	0x00,	// 3F: STOP no sound -> PA
};

static void Votrax_Write(BYTE nDevice, BYTE nValue)
{
	g_bVotraxPhoneme = true;

	// !A/R: Acknowledge receipt of phoneme data (signal goes from high to low)
	SY6522_AY8910* pMB = &g_MB[nDevice];
	UpdateIFR(pMB, IxR_VOTRAX);

	g_nSSI263Device = nDevice;

	SSI263_Play(Votrax2SSI263[nValue & PHONEME_MASK]);
}

//===========================================================================

//#define DBG_MB_UPDATE
static UINT64 g_uLastMBUpdateCycle = 0;

// Called by:
// . MB_UpdateCycles()    - when g_nMBTimerDevice == {0,1,2,3}
// . MB_PeriodicUpdate()  - when g_nMBTimerDevice == kTIMERDEVICE_INVALID
static void MB_UpdateInt(void)
{
	if (!MockingboardVoice.bActive)
		return;

	if (g_bFullSpeed)
	{
		// Keep AY reg writes relative to the current 'frame'
		// - Required for Ultima3:
		//   . Tune ends
		//   . g_bFullSpeed:=true (disk-spinning) for ~50 frames
		//   . U3 sets AY_ENABLE:=0xFF (as a side-effect, this sets g_bFullSpeed:=false)
		//   o Without this, the write to AY_ENABLE gets ignored (since AY8910's /g_uLastCumulativeCycles/ was last set 50 frame ago)
		AY8910UpdateSetCycles();

		// TODO:
		// If any AY regs have changed then push them out to the AY chip

		return;
	}

	//

	if (!g_bMB_RegAccessedFlag)
	{
		if(!g_nMB_InActiveCycleCount)
		{
			g_nMB_InActiveCycleCount = g_nCumulativeCycles;
		}
		else if(g_nCumulativeCycles - g_nMB_InActiveCycleCount > (unsigned __int64)g_fCurrentCLK6502/10)
		{
			// After 0.1 sec of Apple time, assume MB is not active
			g_bMB_Active = false;
		}
	}
	else
	{
		g_nMB_InActiveCycleCount = 0;
		g_bMB_RegAccessedFlag = false;
		g_bMB_Active = true;
	}

	//

	// For small timer periods, wait for a period of 500cy before updating DirectSound ring-buffer.
	// NB. A timer period of less than 24cy will yield nNumSamplesPerPeriod=0.
	const double kMinimumUpdateInterval = 500.0;	// Arbitary (500 cycles = 21 samples)
	const double kMaximumUpdateInterval = (double)(0xFFFF+2);	// Max 6522 timer interval (2756 samples)

	if (g_uLastMBUpdateCycle == 0)
		g_uLastMBUpdateCycle = g_uLastCumulativeCycles;		// Initial call to MB_Update() after reset/power-cycle

	_ASSERT(g_uLastCumulativeCycles >= g_uLastMBUpdateCycle);
	double updateInterval = (double)(g_uLastCumulativeCycles - g_uLastMBUpdateCycle);
	if (updateInterval < kMinimumUpdateInterval)
		return;
	if (updateInterval > kMaximumUpdateInterval)
		updateInterval = kMaximumUpdateInterval;

	g_uLastMBUpdateCycle = g_uLastCumulativeCycles;

	const double nIrqFreq = g_fCurrentCLK6502 / updateInterval + 0.5;			// Round-up
	const int nNumSamplesPerPeriod = (int) ((double)SAMPLE_RATE / nIrqFreq);	// Eg. For 60Hz this is 735

	static int nNumSamplesError = 0;
	int nNumSamples = nNumSamplesPerPeriod + nNumSamplesError;					// Apply correction
	if(nNumSamples <= 0)
		nNumSamples = 0;
	if(nNumSamples > 2*nNumSamplesPerPeriod)
		nNumSamples = 2*nNumSamplesPerPeriod;

	if (nNumSamples > SAMPLE_RATE)
		nNumSamples = SAMPLE_RATE;	// Clamp to prevent buffer overflow (bufferSize = SAMPLE_RATE)

	if(nNumSamples)
		for(int nChip=0; nChip<NUM_AY8910; nChip++)
			AY8910Update(nChip, &ppAYVoiceBuffer[nChip*NUM_VOICES_PER_AY8910], nNumSamples);

	//

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	HRESULT hr = MockingboardVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if(FAILED(hr))
		return;

	static DWORD dwByteOffset = (DWORD)-1;
	if(dwByteOffset == (DWORD)-1)
	{
		// First time in this func

		dwByteOffset = dwCurrentWriteCursor;
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if(dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor))
			{
#ifdef DBG_MB_UPDATE
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X xxx\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#endif
				dwByteOffset = dwCurrentWriteCursor;
				nNumSamplesError = 0;
			}
		}
		else
		{
			// |xxW----------Pxxx|
			if((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
			{
#ifdef DBG_MB_UPDATE
				double fTicksSecs = (double)GetTickCount() / 1000.0;
				LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X XXX\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor-dwCurrentPlayCursor, dwByteOffset, nNumSamples);
#endif
				dwByteOffset = dwCurrentWriteCursor;
				nNumSamplesError = 0;
			}
		}
	}

	int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
	if(nBytesRemaining < 0)
		nBytesRemaining += g_dwDSBufferSize;

	// Calc correction factor so that play-buffer doesn't under/overflow
	const int nErrorInc = SoundCore_GetErrorInc();
	if(nBytesRemaining < g_dwDSBufferSize / 4)
		nNumSamplesError += nErrorInc;				// < 0.25 of buffer remaining
	else if(nBytesRemaining > g_dwDSBufferSize / 2)
		nNumSamplesError -= nErrorInc;				// > 0.50 of buffer remaining
	else
		nNumSamplesError = 0;						// Acceptable amount of data in buffer

#ifdef DBG_MB_UPDATE
	double fTicksSecs = (double)GetTickCount() / 1000.0;
	LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X, NSE=%08X, Interval=%f\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, dwByteOffset, nNumSamples, nNumSamplesError, updateInterval);
#endif

	if(nNumSamples == 0)
		return;

	//

	const double fAttenuation = g_bPhasorEnable ? 2.0/3.0 : 1.0;

	for(int i=0; i<nNumSamples; i++)
	{
		// Mockingboard stereo (all voices on an AY8910 wire-or'ed together)
		// L = Address.b7=0, R = Address.b7=1
		int nDataL = 0, nDataR = 0;

		for(UINT j=0; j<NUM_VOICES_PER_AY8910; j++)
		{
			// Slot4
			nDataL += (int) ((double)ppAYVoiceBuffer[0*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[1*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);

			// Slot5
			nDataL += (int) ((double)ppAYVoiceBuffer[2*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[3*NUM_VOICES_PER_AY8910+j][i] * fAttenuation);
		}

		// Cap the superpositioned output
		if(nDataL < nWaveDataMin)
			nDataL = nWaveDataMin;
		else if(nDataL > nWaveDataMax)
			nDataL = nWaveDataMax;

		if(nDataR < nWaveDataMin)
			nDataR = nWaveDataMin;
		else if(nDataR > nWaveDataMax)
			nDataR = nWaveDataMax;

		g_nMixBuffer[i*g_nMB_NumChannels+0] = (short)nDataL;	// L
		g_nMixBuffer[i*g_nMB_NumChannels+1] = (short)nDataR;	// R
	}

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;

	if(!DSGetLock(MockingboardVoice.lpDSBvoice,
						dwByteOffset, (DWORD)nNumSamples*sizeof(short)*g_nMB_NumChannels,
						&pDSLockedBuffer0, &dwDSLockedBufferSize0,
						&pDSLockedBuffer1, &dwDSLockedBufferSize1))
		return;

	memcpy(pDSLockedBuffer0, &g_nMixBuffer[0], dwDSLockedBufferSize0);
	if(pDSLockedBuffer1)
		memcpy(pDSLockedBuffer1, &g_nMixBuffer[dwDSLockedBufferSize0/sizeof(short)], dwDSLockedBufferSize1);

	// Commit sound buffer
	hr = MockingboardVoice.lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
											  (void*)pDSLockedBuffer1, dwDSLockedBufferSize1);

	dwByteOffset = (dwByteOffset + (DWORD)nNumSamples*sizeof(short)*g_nMB_NumChannels) % g_dwDSBufferSize;

#ifdef RIFF_MB
	RiffPutSamples(&g_nMixBuffer[0], nNumSamples);
#endif
}

static void MB_Update(void)
{
#ifdef LOG_PERF_TIMINGS
	extern UINT64 g_timeMB_NoTimer;
	extern UINT64 g_timeMB_Timer;
	PerfMarker perfMarker(g_nMBTimerDevice == kTIMERDEVICE_INVALID ? g_timeMB_NoTimer : g_timeMB_Timer);
#endif

	MB_UpdateInt();
}

//-----------------------------------------------------------------------------

// Called by SSI263Thread(), MB_LoadSnapshot & Phasor_LoadSnapshot
// Pre: g_bVotraxPhoneme, g_bPhasorEnable, g_phasorMode
static void SetSpeechIRQ(SY6522_AY8910* pMB)
{
	if (!g_bVotraxPhoneme)
	{
		// Always set SSI263's D7 pin regardless of SSI263 mode (DR1:0), including MODE_IRQ_DISABLED
		pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin

		if ((pMB->SpeechChip.CurrentMode & DURATION_MODE_MASK) != MODE_IRQ_DISABLED)
		{
			if (!g_bPhasorEnable || (g_bPhasorEnable && g_phasorMode == PH_Mockingboard))
			{
				if ((pMB->sy6522.PCR & 1) == 0)			// CA1 Latch/Input = 0 (Negative active edge)
					UpdateIFR(pMB, 0, IxR_PERIPHERAL);
				if (pMB->sy6522.PCR == 0x0C)			// CA2 Control = b#110 (Low output)
					pMB->SpeechChip.CurrentMode &= ~1;	// Clear SSI263's D7 pin (cleared by 6522's PCR CA1/CA2 handshake)

				// NB. Don't set CTL=1, as Mockingboard(SMS) speech doesn't work (it sets MODE_IRQ_DISABLED mode during ISR)
				//pMB->SpeechChip.CtrlArtAmp |= CONTROL_MASK;	// 6522's CA2 sets Power Down mode (pin 18), which sets Control bit
			}
			else if (g_bPhasorEnable && g_phasorMode == PH_Phasor)	// Phasor's SSI263 IRQ (A/!R) line is *also* wired directly to the 6502's IRQ (as well as the 6522's CA1)
			{
				CpuIrqAssert(IS_SPEECH);
			}
		}
	}

	//

	if (g_bVotraxPhoneme && pMB->sy6522.PCR == 0xB0)
	{
		// !A/R: Time-out of old phoneme (signal goes from low to high)

		UpdateIFR(pMB, 0, IxR_VOTRAX);

		g_bVotraxPhoneme = false;
	}
}

static DWORD WINAPI SSI263Thread(LPVOID lpParameter)
{
	while(1)
	{
		DWORD dwWaitResult = WaitForMultipleObjects( 
								g_nNumEvents,		// number of handles in array
								g_hSSI263Event,		// array of event handles
								FALSE,				// wait until any one is signaled
								INFINITE);

		if((dwWaitResult < WAIT_OBJECT_0) || (dwWaitResult > WAIT_OBJECT_0+g_nNumEvents-1))
			continue;

		dwWaitResult -= WAIT_OBJECT_0;			// Determine event # that signaled

		if(dwWaitResult == (g_nNumEvents-1))	// Termination event
			break;

		// Phoneme completed playing

		if (g_bStopPhoneme)
		{
			g_bStopPhoneme = false;
			continue;
		}

#if LOG_SSI263
		//if(g_fh) fprintf(g_fh, "IRQ: Phoneme complete (0x%02X)\n\n", g_nCurrentActivePhoneme);
#endif

		if (g_nCurrentActivePhoneme < 0)
			continue;	// On CTRL+RESET or power-cycle (during phoneme playback): ResetState() is called, which set g_nCurrentActivePhoneme=-1

		SSI263Voice[g_nCurrentActivePhoneme].bActive = false;
		g_nCurrentActivePhoneme = -1;

		// Phoneme complete, so generate IRQ if necessary
		SY6522_AY8910* pMB = &g_MB[g_nSSI263Device];
		SetSpeechIRQ(pMB);
	}

	return 0;
}

//-----------------------------------------------------------------------------

static void SSI263_Play(unsigned int nPhoneme)
{
#if 1
	HRESULT hr;

	{
		int nCurrPhoneme = g_nCurrentActivePhoneme;	// local copy in case SSI263Thread sets it to -1
		if (nCurrPhoneme >= 0)
		{
			// A write to DURPHON before previous phoneme has completed
			g_bStopPhoneme = true;
			hr = SSI263Voice[nCurrPhoneme].lpDSBvoice->Stop();

			// Busy-wait until ACK from SSI263Thread
			// . required to avoid data-race
			while (	g_bStopPhoneme &&				// wait for SSI263Thread to ACK the lpDSBVoice->Stop()
					g_nCurrentActivePhoneme >= 0)	// wait for SSI263Thread to get end of sample event
				;

			g_bStopPhoneme = false;
		}
	}

	g_nCurrentActivePhoneme = nPhoneme;

	hr = SSI263Voice[g_nCurrentActivePhoneme].lpDSBvoice->SetCurrentPosition(0);
	if(FAILED(hr))
		return;

	hr = SSI263Voice[g_nCurrentActivePhoneme].lpDSBvoice->Play(0,0,0);	// Not looping
	if(FAILED(hr))
		return;

	SSI263Voice[g_nCurrentActivePhoneme].bActive = true;
#else
	HRESULT hr;
	bool bPause;

	if(nPhoneme == 1)
		nPhoneme = 2;	// Missing this sample, so map to phoneme-2

	if(nPhoneme == 0)
	{
		bPause = true;
	}
	else
	{
//		nPhoneme--;
		nPhoneme-=2;	// Missing phoneme-1
		bPause = false;
	}

	DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	SHORT* pDSLockedBuffer;

	hr = SSI263Voice.lpDSBvoice->Stop();

	if(!DSGetLock(SSI263Voice.lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0))
		return;

	unsigned int nPhonemeShortLength = g_nPhonemeInfo[nPhoneme].nLength;
	unsigned int nPhonemeByteLength = g_nPhonemeInfo[nPhoneme].nLength * sizeof(SHORT);

	if(bPause)
	{
		// 'pause' length is length of 1st phoneme (arbitrary choice, since don't know real length)
		memset(pDSLockedBuffer, 0, g_dwMaxPhonemeLen);
	}
	else
	{
		memcpy(pDSLockedBuffer, &g_nPhonemeData[g_nPhonemeInfo[nPhoneme].nOffset], nPhonemeByteLength);
		memset(&pDSLockedBuffer[nPhonemeShortLength], 0, g_dwMaxPhonemeLen-nPhonemeByteLength);
	}

#if 0
	DSBPOSITIONNOTIFY PositionNotify;

	PositionNotify.dwOffset = nPhonemeByteLength - 1;		// End of phoneme
	PositionNotify.hEventNotify = g_hSSI263Event[0];

	hr = SSI263Voice.lpDSNotify->SetNotificationPositions(1, &PositionNotify);
	if(FAILED(hr))
	{
		DirectSound_ErrorText(hr);
		return;
	}
#endif

	hr = SSI263Voice.lpDSBvoice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
	if(FAILED(hr))
		return;

	hr = SSI263Voice.lpDSBvoice->Play(0,0,0);	// Not looping
	if(FAILED(hr))
		return;

	SSI263Voice.bActive = true;
#endif
}

//-----------------------------------------------------------------------------

static bool MB_DSInit()
{
	LogFileOutput("MB_DSInit\n", g_bMBAvailable);
#ifdef NO_DIRECT_X

	return false;

#else // NO_DIRECT_X

	//
	// Create single Mockingboard voice
	//

	DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	SHORT* pDSLockedBuffer;

	if(!g_bDSAvailable)
		return false;

	HRESULT hr = DSGetSoundBuffer(&MockingboardVoice, DSBCAPS_CTRLVOLUME, g_dwDSBufferSize, SAMPLE_RATE, 2);
	LogFileOutput("MB_DSInit: DSGetSoundBuffer(), hr=0x%08X\n", hr);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "MB: DSGetSoundBuffer failed (%08X)\n",hr);
		return false;
	}

	bool bRes = DSZeroVoiceBuffer(&MockingboardVoice, "MB", g_dwDSBufferSize);
	LogFileOutput("MB_DSInit: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
	if (!bRes)
		return false;

	MockingboardVoice.bActive = true;

	// Volume might've been setup from value in Registry
	if(!MockingboardVoice.nVolume)
		MockingboardVoice.nVolume = DSBVOLUME_MAX;

	hr = MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
	LogFileOutput("MB_DSInit: SetVolume(), hr=0x%08X\n", hr);

	//---------------------------------

	//
	// Create SSI263 voice
	//

#if 0
	g_dwMaxPhonemeLen = 0;
	for(int i=0; i<sizeof(g_nPhonemeInfo) / sizeof(PHONEME_INFO); i++)
		if(g_dwMaxPhonemeLen < g_nPhonemeInfo[i].nLength)
			g_dwMaxPhonemeLen = g_nPhonemeInfo[i].nLength;
	g_dwMaxPhonemeLen *= sizeof(SHORT);
#endif

	g_hSSI263Event[0] = CreateEvent(NULL,	// lpEventAttributes
									FALSE,	// bManualReset (FALSE = auto-reset)
									FALSE,	// bInitialState (FALSE = non-signaled)
									NULL);	// lpName
	LogFileOutput("MB_DSInit: CreateEvent(), g_hSSI263Event[0]=0x%08X\n", g_hSSI263Event[0]);

	g_hSSI263Event[1] = CreateEvent(NULL,	// lpEventAttributes
									FALSE,	// bManualReset (FALSE = auto-reset)
									FALSE,	// bInitialState (FALSE = non-signaled)
									NULL);	// lpName
	LogFileOutput("MB_DSInit: CreateEvent(), g_hSSI263Event[1]=0x%08X\n", g_hSSI263Event[1]);

	if((g_hSSI263Event[0] == NULL) || (g_hSSI263Event[1] == NULL))
	{
		if(g_fh) fprintf(g_fh, "SSI263: CreateEvent failed\n");
		return false;
	}

	for(int i=0; i<64; i++)
	{
		unsigned int nPhoneme = i;
		bool bPause;

		if(nPhoneme == 1)
			nPhoneme = 2;	// Missing this sample, so map to phoneme-2

		if(nPhoneme == 0)
		{
			bPause = true;
		}
		else
		{
//			nPhoneme--;
			nPhoneme-=2;	// Missing phoneme-1
			bPause = false;
		}

		unsigned int nPhonemeByteLength = g_nPhonemeInfo[nPhoneme].nLength * sizeof(SHORT);

		// NB. DSBCAPS_LOCSOFTWARE required for Phoneme+2==0x28 - sample too short (see KB327698)
		hr = DSGetSoundBuffer(&SSI263Voice[i], DSBCAPS_CTRLVOLUME+DSBCAPS_CTRLPOSITIONNOTIFY+DSBCAPS_LOCSOFTWARE, nPhonemeByteLength, 22050, 1);
		LogFileOutput("MB_DSInit: (%02d) DSGetSoundBuffer(), hr=0x%08X\n", i, hr);
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: DSGetSoundBuffer failed (%08X)\n",hr);
			return false;
		}

		bRes = DSGetLock(SSI263Voice[i].lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0);
		//LogFileOutput("MB_DSInit: (%02d) DSGetLock(), res=%d\n", i, bRes ? 1 : 0);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: DSGetLock failed (%08X)\n",hr);
			return false;
		}

		if(bPause)
		{
			// 'pause' length is length of 1st phoneme (arbitrary choice, since don't know real length)
			memset(pDSLockedBuffer, 0x00, nPhonemeByteLength);
		}
		else
		{
			memcpy(pDSLockedBuffer, &g_nPhonemeData[g_nPhonemeInfo[nPhoneme].nOffset], nPhonemeByteLength);
		}

 		hr = SSI263Voice[i].lpDSBvoice->QueryInterface(IID_IDirectSoundNotify, (LPVOID *)&SSI263Voice[i].lpDSNotify);
		//LogFileOutput("MB_DSInit: (%02d) QueryInterface(), hr=0x%08X\n", i, hr);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: QueryInterface failed (%08X)\n",hr);
			return false;
		}

		DSBPOSITIONNOTIFY PositionNotify;

//		PositionNotify.dwOffset = nPhonemeByteLength - 1;	// End of buffer
		PositionNotify.dwOffset = DSBPN_OFFSETSTOP;			// End of buffer
		PositionNotify.hEventNotify = g_hSSI263Event[0];

		hr = SSI263Voice[i].lpDSNotify->SetNotificationPositions(1, &PositionNotify);
		//LogFileOutput("MB_DSInit: (%02d) SetNotificationPositions(), hr=0x%08X\n", i, hr);	// WARNING: Lock acquired && doing heavy-weight logging
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: SetNotifyPos failed (%08X)\n",hr);
			return false;
		}

		hr = SSI263Voice[i].lpDSBvoice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
		LogFileOutput("MB_DSInit: (%02d) Unlock(),hr=0x%08X\n", i, hr);
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: DSUnlock failed (%08X)\n",hr);
			return false;
		}

		SSI263Voice[i].bActive = false;
		SSI263Voice[i].nVolume = MockingboardVoice.nVolume;		// Use same volume as MB
		hr = SSI263Voice[i].lpDSBvoice->SetVolume(SSI263Voice[i].nVolume);
		LogFileOutput("MB_DSInit: (%02d) SetVolume(), hr=0x%08X\n", i, hr);
	}

	//

	DWORD dwThreadId;

	g_hThread = CreateThread(NULL,				// lpThreadAttributes
								0,				// dwStackSize
								SSI263Thread,
								NULL,			// lpParameter
								0,				// dwCreationFlags : 0 = Run immediately
								&dwThreadId);	// lpThreadId
	LogFileOutput("MB_DSInit: CreateThread(), g_hThread=0x%08X\n", g_hThread);

	BOOL bRes2 = SetThreadPriority(g_hThread, THREAD_PRIORITY_TIME_CRITICAL);
	LogFileOutput("MB_DSInit: SetThreadPriority(), bRes=%d\n", bRes2 ? 1 : 0);

	return true;

#endif // NO_DIRECT_X
}

static void MB_DSUninit()
{
	if(g_hThread)
	{
		DWORD dwExitCode;
		SetEvent(g_hSSI263Event[g_nNumEvents-1]);	// Signal to thread that it should exit

		do
		{
			if(GetExitCodeThread(g_hThread, &dwExitCode))
			{
				if(dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		}
		while(1);

		CloseHandle(g_hThread);
		g_hThread = NULL;
	}

	//

	if(MockingboardVoice.lpDSBvoice && MockingboardVoice.bActive)
	{
		MockingboardVoice.lpDSBvoice->Stop();
		MockingboardVoice.bActive = false;
	}

	DSReleaseSoundBuffer(&MockingboardVoice);

	//

	for(int i=0; i<64; i++)
	{
		if(SSI263Voice[i].lpDSBvoice && SSI263Voice[i].bActive)
		{
			SSI263Voice[i].lpDSBvoice->Stop();
			SSI263Voice[i].bActive = false;
		}

		DSReleaseSoundBuffer(&SSI263Voice[i]);
	}

	//

	if(g_hSSI263Event[0])
	{
		CloseHandle(g_hSSI263Event[0]);
		g_hSSI263Event[0] = NULL;
	}

	if(g_hSSI263Event[1])
	{
		CloseHandle(g_hSSI263Event[1]);
		g_hSSI263Event[1] = NULL;
	}
}

//=============================================================================

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//=============================================================================

static void InitSoundcardType(void)
{
	g_SoundcardType = CT_Empty;	// Use CT_Empty to mean: no soundcard
	g_bPhasorEnable = false;
}

void MB_Initialize()
{
	InitSoundcardType();

	LogFileOutput("MB_Initialize: g_bDisableDirectSound=%d, g_bDisableDirectSoundMockingboard=%d\n", g_bDisableDirectSound, g_bDisableDirectSoundMockingboard);
	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
	{
		MockingboardVoice.bMute = true;
	}
	else
	{
		memset(&g_MB,0,sizeof(g_MB));

		int i;
		for(i=0; i<NUM_VOICES; i++)
			ppAYVoiceBuffer[i] = new short [SAMPLE_RATE];	// Buffer can hold a max of 1 seconds worth of samples

		AY8910_InitAll((int)g_fCurrentCLK6502, SAMPLE_RATE);
		LogFileOutput("MB_Initialize: AY8910_InitAll()\n");

		for(i=0; i<NUM_AY8910; i++)
			g_MB[i].nAY8910Number = i;

		//

		g_bMBAvailable = MB_DSInit();
		LogFileOutput("MB_Initialize: MB_DSInit(), g_bMBAvailable=%d\n", g_bMBAvailable);

		MB_Reset();
		LogFileOutput("MB_Initialize: MB_Reset()\n");
	}

	InitializeCriticalSection(&g_CriticalSection);
	g_bCritSectionValid = true;

	for (int id=0; id<kNumSyncEvents; id++)
	{
		g_syncEvent[id] = new SyncEvent(id, 0, MB_SyncEventCallback);
	}
}

static void MB_SetSoundcardType(SS_CARDTYPE NewSoundcardType);

// NB. Mockingboard voice is *already* muted because showing 'Select Load State file' dialog
// . and voice will be demuted when dialog is closed
void MB_InitializeForLoadingSnapshot()	// GH#609
{
	MB_Reset();
	InitSoundcardType();

	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

	_ASSERT(MockingboardVoice.lpDSBvoice);
	MockingboardVoice.lpDSBvoice->Stop();	// Reason: 'MB voice is playing' then loading a save-state where 'no MB present'
}

//-----------------------------------------------------------------------------

// NB. Called when /g_fCurrentCLK6502/ changes
void MB_Reinitialize()
{
	AY8910_InitClock((int)g_fCurrentCLK6502);	// todo: account for g_PhasorClockScaleFactor?
												// NB. Other calls to AY8910_InitClock() use the constant CLK_6502
}

//-----------------------------------------------------------------------------

void MB_Destroy()
{
	MB_DSUninit();

	for (int i=0; i<NUM_VOICES; i++)
		delete [] ppAYVoiceBuffer[i];

	if (g_bCritSectionValid)
	{
		DeleteCriticalSection(&g_CriticalSection);
		g_bCritSectionValid = false;
	}

	for (int id=0; id<kNumSyncEvents; id++)
	{
		if (g_syncEvent[id] && g_syncEvent[id]->m_active)
			g_SynchronousEventMgr.Remove(id);

		delete g_syncEvent[id];
		g_syncEvent[id] = NULL;
	}
}

//-----------------------------------------------------------------------------

static void ResetState()
{
	g_nMBTimerDevice = kTIMERDEVICE_INVALID;
	MB_SetCumulativeCycles();

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;
	g_bStopPhoneme = false;
	g_bVotraxPhoneme = false;

	g_nMB_InActiveCycleCount = 0;
	g_bMB_RegAccessedFlag = false;
	g_bMB_Active = false;

	g_phasorMode = PH_Mockingboard;
	g_PhasorClockScaleFactor = 1;

	g_uLastMBUpdateCycle = 0;
	g_cyclesThisAudioFrame = 0;

	for (int id = 0; id < kNumSyncEvents; id++)
	{
		if (g_syncEvent[id] && g_syncEvent[id]->m_active)
			g_SynchronousEventMgr.Remove(id);
	}

	// Not these, as they don't change on a CTRL+RESET or power-cycle:
//	g_bMBAvailable = false;
//	g_SoundcardType = CT_Empty;	// Don't uncomment, else _ASSERT will fire in MB_Read() after an F2->MB_Reset()
//	g_bPhasorEnable = false;
}

void MB_Reset()	// CTRL+RESET or power-cycle
{
	if(!g_bDSAvailable)
		return;

	for(int i=0; i<NUM_AY8910; i++)
	{
		ResetSY6522(&g_MB[i]);
		AY8910_reset(i);
	}

	ResetState();
	MB_Reinitialize();	// Reset CLK for AY8910s
}

//-----------------------------------------------------------------------------

// Echo+ mode - Phasor's 2nd 6522 is mapped to every 16-byte offset in $Cnxx (Echo+ has a single 6522 controlling two AY-3-8913's)

static BYTE __stdcall MB_Read(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	MB_UpdateCycles(nExecutedCycles);

#ifdef _DEBUG
	if(!IS_APPLE2 && MemCheckINTCXROM())
	{
		_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
		return mem[nAddr];
	}

	if(g_SoundcardType == CT_Empty)
	{
		_ASSERT(0);	// Card unplugged, so IO_Cxxx() returns the floating bus
		return MemReadFloatingBus(nExecutedCycles);
	}
#endif

	BYTE nMB = (nAddr>>8)&0xf - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
			return MemReadFloatingBus(nExecutedCycles);

		int CS = 0;
		if (g_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (g_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (g_phasorMode == PH_EchoPlus)
			CS = 2;

		BYTE nRes = 0;

		if(CS & 1)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);

		if(CS & 2)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);

		bool bAccessedDevice = (CS & 3) ? true : false;

		if ((g_phasorMode == PH_Phasor) && ((nAddr & 0xD0) == 0x40))	// $Cn4x and $Cn6x (Mockingboard mode: SSI263.bit7 not readable)
		{
			_ASSERT(!bAccessedDevice);
			nRes = SSI263_Read(nMB*2+1, nExecutedCycles);		// SSI263 only drives bit7
			bAccessedDevice = true;
		}

		return bAccessedDevice ? nRes : MemReadFloatingBus(nExecutedCycles);
	}

	// NB. Mockingboard: SSI263.bit7 not readable (TODO: check this with real h/w)
	if (nOffset < SY6522B_Offset)
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);
	else
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);
}

//-----------------------------------------------------------------------------

static BYTE __stdcall MB_Write(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	MB_UpdateCycles(nExecutedCycles);

#ifdef _DEBUG
	if(!IS_APPLE2 && MemCheckINTCXROM())
	{
		_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
		return 0;
	}

	if(g_SoundcardType == CT_Empty)
	{
		_ASSERT(0);	// Card unplugged, so IO_Cxxx() returns the floating bus
		return 0;
	}
#endif

	// Support 6502/65C02 false-reads of 6522 (GH#52)
	if ( ((mem[(PC-2)&0xffff] == 0x91) && GetMainCpu() == CPU_6502) ||	// sta (zp),y - 6502 only (no-PX variant only) (UTAIIe:4-23)
		 (mem[(PC-3)&0xffff] == 0x99) ||	// sta abs16,y - 6502/65C02, but for 65C02 only the no-PX variant that does the false-read (UTAIIe:4-27)
		 (mem[(PC-3)&0xffff] == 0x9D) )		// sta abs16,x - 6502/65C02, but for 65C02 only the no-PX variant that does the false-read (UTAIIe:4-27)
	{
		WORD base;
		WORD addr16;
		if (mem[(PC-2)&0xffff] == 0x91)
		{
			BYTE zp = mem[(PC-1)&0xffff];
			base = (mem[zp] | (mem[(zp+1)&0xff]<<8));
			addr16 = base + regs.y;
		}
		else
		{
			base = mem[(PC-2)&0xffff] | (mem[(PC-1)&0xffff]<<8);
			addr16 = base + ((mem[(PC-3)&0xffff] == 0x99) ? regs.y : regs.x);
		}

		if (((base ^ addr16) >> 8) == 0)	// Only the no-PX variant does the false read (to the same I/O SELECT page)
		{
			_ASSERT(addr16 == nAddr);
			if (addr16 == nAddr)	// Check we've reverse looked-up the 6502 opcode correctly
			{
				if ( ((nAddr&0xf) == 4) || ((nAddr&0xf) == 8) )	// Only reading 6522 reg-4 or reg-8 actually has an effect
					MB_Read(PC, nAddr, 0, 0, nExecutedCycles);
			}
		}
	}

	BYTE nMB = ((nAddr>>8)&0xf) - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
			return 0;

		int CS = 0;
		if (g_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (g_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (g_phasorMode == PH_EchoPlus)
			CS = 2;

		if(CS & 1)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);

		if(CS & 2)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);

		int CS_SSI263 =   (g_phasorMode == PH_Mockingboard)	? (nAddr & 0xE0) == 0x40	// Mockingboard: $Cn4x
						: (g_phasorMode == PH_Phasor)		? (nAddr & 0xC0) == 0x40	// Phasor:       $Cn4x and $Cn6x
						: 0;															// Echo+

		if (CS_SSI263)
		{
			// NB. Mockingboard mode: writes to $Cn4x/SSI263 also get written to 1st 6522 (have confirmed on real Phasor h/w)
			_ASSERT( (g_phasorMode == PH_Mockingboard && (CS==0 || CS==1)) || (g_phasorMode == PH_Phasor && (CS==0)) );
			SSI263_Write(nMB*2+1, nAddr&0x7, nValue);	// Second 6522 is used for speech chip
		}

		return 0;
	}

	if (nOffset < SY6522B_Offset)
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);
	else
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);

	if ((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x07)))
		SSI263_Write(nMB*2+1, nAddr&0x7, nValue);		// Second 6522 is used for speech chip -- TODO confirm with real MB h/w that writes go to 1st 6522

	return 0;
}

//-----------------------------------------------------------------------------

// Phasor's DEVICE SELECT' logic:
// . if addr.[b3]==1, then clear the card's mode bits b2:b0
// . if any of addr.[b2:b0] are a logic 1, then set these bits in the card's mode
//
// Example DEVICE SELECT' accesses for Phasor in slot-4: (from empirical observations on real Phasor h/w)
// 1)
// . RESET -> Mockingboard mode (b#000)
// . $C0C5 -> Phasor mode (b#101)
// 2)
// . RESET -> Mockingboard mode (b#000)
// . $C0C1, then $C0C4  (or $C0C4, then $C0C1) -> Phasor mode (b#101)
// . $C0C2 -> Echo+ mode (b#111)
// . $C0C5 -> remaing in Echo+ mode (b#111)
// So $C0C5 seemingly results in 2 different modes.
//

static BYTE __stdcall PhasorIO(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	if (!g_bPhasorEnable)
		return MemReadFloatingBus(nExecutedCycles);

	UINT bits = (UINT) g_phasorMode;
	if (nAddr & 8)
		bits = 0;
	bits |= (nAddr & 7);
	g_phasorMode = (PHASOR_MODE) bits;

	if (g_phasorMode == PH_Mockingboard || g_phasorMode == PH_EchoPlus)
		g_PhasorClockScaleFactor = 1;
	else if (g_phasorMode == PH_Phasor)
		g_PhasorClockScaleFactor = 2;

	AY8910_InitClock((int)(Get6502BaseClock() * g_PhasorClockScaleFactor));

	return MemReadFloatingBus(nExecutedCycles);
}

//-----------------------------------------------------------------------------

SS_CARDTYPE MB_GetSoundcardType()
{
	return g_SoundcardType;
}

static void MB_SetSoundcardType(const SS_CARDTYPE NewSoundcardType)
{
	if (NewSoundcardType == g_SoundcardType)
		return;

	if (NewSoundcardType == CT_Empty)
		MB_Mute();	// Call MB_Mute() before setting g_SoundcardType = CT_Empty

	g_SoundcardType = NewSoundcardType;

	g_bPhasorEnable = (g_SoundcardType == CT_Phasor);
}

//-----------------------------------------------------------------------------

void MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5)
{
	// Mockingboard: Slot 4 & 5
	// Phasor      : Slot 4
	// <other>     : Slot 4 & 5

	if (GetCardMgr().QuerySlot(SLOT4) != CT_MockingboardC && GetCardMgr().QuerySlot(SLOT4) != CT_Phasor)
	{
		MB_SetSoundcardType(CT_Empty);
		return;
	}

	if (GetCardMgr().QuerySlot(SLOT4) == CT_MockingboardC)
		RegisterIoHandler(uSlot4, IO_Null, IO_Null, MB_Read, MB_Write, NULL, NULL);
	else	// Phasor
		RegisterIoHandler(uSlot4, PhasorIO, PhasorIO, MB_Read, MB_Write, NULL, NULL);

	if (GetCardMgr().QuerySlot(SLOT5) == CT_MockingboardC)
		RegisterIoHandler(uSlot5, IO_Null, IO_Null, MB_Read, MB_Write, NULL, NULL);

	MB_SetSoundcardType(GetCardMgr().QuerySlot(SLOT4));

	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

	// Sound buffer may have been stopped by MB_InitializeForLoadingSnapshot().
	// NB. DSZeroVoiceBuffer() also zeros the sound buffer, so it's better than directly calling IDirectSoundBuffer::Play():
	// - without zeroing, then the previous sound buffer can be heard for a fraction of a second
	// - eg. when doing Mockingboard playback, then loading a save-state which is also doing Mockingboard playback
	DSZeroVoiceBuffer(&MockingboardVoice, "MB", g_dwDSBufferSize);
}

//-----------------------------------------------------------------------------

void MB_Mute()
{
	if(g_SoundcardType == CT_Empty)
		return;

	if(MockingboardVoice.bActive && !MockingboardVoice.bMute)
	{
		MockingboardVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
		MockingboardVoice.bMute = true;
	}

	if(g_nCurrentActivePhoneme >= 0)
		SSI263Voice[g_nCurrentActivePhoneme].lpDSBvoice->SetVolume(DSBVOLUME_MIN);
}

//-----------------------------------------------------------------------------

void MB_Demute()
{
	if(g_SoundcardType == CT_Empty)
		return;

	if(MockingboardVoice.bActive && MockingboardVoice.bMute)
	{
		MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
		MockingboardVoice.bMute = false;
	}

	if(g_nCurrentActivePhoneme >= 0)
		SSI263Voice[g_nCurrentActivePhoneme].lpDSBvoice->SetVolume(SSI263Voice[g_nCurrentActivePhoneme].nVolume);
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG
void MB_CheckCumulativeCycles()
{
	if (g_SoundcardType == CT_Empty)
		return;

	_ASSERT(g_uLastCumulativeCycles == g_nCumulativeCycles);
	g_uLastCumulativeCycles = g_nCumulativeCycles;
}
#endif

// Called by: ResetState() and Snapshot_LoadState_v2()
void MB_SetCumulativeCycles()
{
	g_uLastCumulativeCycles = g_nCumulativeCycles;
}

// Called by ContinueExecution() at the end of every execution period (~1000 cycles or ~3 cycle when MODE_STEPPING)
// NB. Required for FT's TEST LAB #1 player
void MB_PeriodicUpdate(UINT executedCycles)
{
	if (g_SoundcardType == CT_Empty)
		return;

	if (g_nMBTimerDevice != kTIMERDEVICE_INVALID)
		return;

	const UINT kCyclesPerAudioFrame = 1000;
	g_cyclesThisAudioFrame += executedCycles;
	if (g_cyclesThisAudioFrame < kCyclesPerAudioFrame)
		return;

	g_cyclesThisAudioFrame %= kCyclesPerAudioFrame;

	MB_Update();
}

//-----------------------------------------------------------------------------

static bool CheckTimerUnderflow(USHORT& timerCounter, int& timerIrqDelay, const USHORT nClocks)
{
	if (nClocks == 0)
		return false;

	int oldTimer = timerCounter;
	int timer = timerCounter;
	timer -= nClocks;
	timerCounter = (USHORT)timer;

	bool timerIrq = false;

	if (timerIrqDelay)	// Deal with any previous counter underflow which didn't yet result in an IRQ
	{
		_ASSERT(timerIrqDelay == 1);
		timerIrqDelay = 0;
		timerIrq = true;
		// if LATCH is very small then could underflow for every opcode...
	}

	if (oldTimer >= 0 && timer < 0)	// Underflow occurs for 0x0000 -> 0xFFFF
	{
		if (timer <= -2)				// TIMER = 0xFFFE (or less)
			timerIrq = true;
		else							// TIMER = 0xFFFF
			timerIrqDelay = 1;			// ...so 1 cycle until IRQ
	}

	return timerIrq;
}

// Called by:
// . CpuExecute() every ~1000 cycles @ 1MHz
// . MB_SyncEventCallback() on a TIMER1/2 underflow
// . MB_Read() / MB_Write() (for both normal & full-speed)
void MB_UpdateCycles(ULONG uExecutedCycles)
{
	if (g_SoundcardType == CT_Empty)
		return;

	CpuCalcCycles(uExecutedCycles);
	UINT64 uCycles = g_nCumulativeCycles - g_uLastCumulativeCycles;
	_ASSERT(uCycles >= 0);
	if (uCycles == 0)
		return;

	g_uLastCumulativeCycles = g_nCumulativeCycles;
	_ASSERT(uCycles < 0x10000 || g_nAppMode == MODE_BENCHMARK);
	USHORT nClocks = (USHORT)uCycles;

	for (int i = 0; i < NUM_SY6522; i++)
	{
		SY6522_AY8910* pMB = &g_MB[i];

		const bool bTimer1Underflow = CheckTimerUnderflow(pMB->sy6522.TIMER1_COUNTER.w, pMB->sy6522.timer1IrqDelay, nClocks);
		const bool bTimer2Underflow = CheckTimerUnderflow(pMB->sy6522.TIMER2_COUNTER.w, pMB->sy6522.timer2IrqDelay, nClocks);

		if (pMB->bTimer1Active && bTimer1Underflow)
		{
			pMB->sy6522.TIMER1_COUNTER.w += pMB->sy6522.TIMER1_LATCH.w;	// GH#651: account for underflowed cycles too
			pMB->sy6522.TIMER1_COUNTER.w += kExtraTimerCycles;			// GH#652: account for extra 2 cycles
																		// EG. T1C=0xFFFE, T1L=0x0001
																		// . T1C += T1L = 0xFFFF
																		// . T1C +=   2 = 0x0001
			if (pMB->sy6522.TIMER1_COUNTER.w > pMB->sy6522.TIMER1_LATCH.w)
			{
				if (pMB->sy6522.TIMER1_LATCH.w)
					pMB->sy6522.TIMER1_COUNTER.w %= pMB->sy6522.TIMER1_LATCH.w;	// Only occurs if LATCH.w<0x0007 (# cycles for longest opcode)
				else
					pMB->sy6522.TIMER1_COUNTER.w = 0;
			}
		}
	}
}

//-----------------------------------------------------------------------------

static int MB_SyncEventCallback(int id, int /*cycles*/, ULONG uExecutedCycles)
{
	SY6522_AY8910* pMB = &g_MB[id / kNumTimersPer6522];

	if ((id & 1) == 0)
	{
		_ASSERT(pMB->bTimer1Active);
		MB_Update();

		UpdateIFR(pMB, 0, IxR_TIMER1);

		if ((pMB->sy6522.ACR & RUNMODE) == RM_ONESHOT)
		{
			// One-shot mode
			// - Phasor's playback code uses one-shot mode
			StopTimer1(pMB);
			return 0;			// Don't repeat event
		}

		MB_UpdateCycles(uExecutedCycles);

		StartTimer1(pMB);
		return pMB->sy6522.TIMER1_COUNTER.w + kExtraTimerCycles;
	}
	else
	{
		_ASSERT(pMB->bTimer2Active);
		UpdateIFR(pMB, 0, IxR_TIMER2);

		StopTimer2(pMB);	// TIMER2 only runs in one-shot mode
		return 0;			// Don't repeat event
	}
}

//-----------------------------------------------------------------------------

bool MB_IsActive()
{
	if (!MockingboardVoice.bActive)
		return false;

	return g_bMB_Active;
}

//-----------------------------------------------------------------------------

DWORD MB_GetVolume()
{
	return MockingboardVoice.dwUserVolume;
}

void MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax)
{
	MockingboardVoice.dwUserVolume = dwVolume;

	MockingboardVoice.nVolume = NewVolume(dwVolume, dwVolumeMax);

	if (MockingboardVoice.bActive && !MockingboardVoice.bMute)
		MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
}

//===========================================================================

// Called by debugger - Debugger_Display.cpp
void MB_GetSnapshot_v1(SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot)
{
	pSS->Hdr.UnitHdr.hdr.v2.Length = sizeof(SS_CARD_MOCKINGBOARD_v1);
	pSS->Hdr.UnitHdr.hdr.v2.Type = UT_Card;
	pSS->Hdr.UnitHdr.hdr.v2.Version = 1;

	pSS->Hdr.Slot = dwSlot;
	pSS->Hdr.Type = CT_MockingboardC;

	UINT nMbCardNum = dwSlot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	for(UINT i=0; i<MB_UNITS_PER_CARD_v1; i++)
	{
		memcpy(&pSS->Unit[i].RegsSY6522, &pMB->sy6522, sizeof(SY6522));
		memcpy(&pSS->Unit[i].RegsAY8910, AY8910_GetRegsPtr(nDeviceNum), 16);
		memcpy(&pSS->Unit[i].RegsSSI263, &pMB->SpeechChip, sizeof(SSI263A));
		pSS->Unit[i].nAYCurrentRegister = pMB->nAYCurrentRegister;
		pSS->Unit[i].bTimer1IrqPending = false;
		pSS->Unit[i].bTimer2IrqPending = false;
		pSS->Unit[i].bSpeechIrqPending = false;

		nDeviceNum++;
		pMB++;
	}
}

//===========================================================================

// Unit version history:
// 2: Added: Timer1 & Timer2 active
// 3: Added: Unit state - GH#320
// 4: Added: 6522 timerIrqDelay - GH#652
// 5: Added: Unit state-B (Phasor only) - GH#659
// 6: Changed SS_YAML_KEY_PHASOR_MODE from (0,1) to (0,5,7)
//    Added SS_YAML_KEY_VOTRAX_PHONEME
//    Removed: redundant SS_YAML_KEY_PHASOR_CLOCK_SCALE_FACTOR
const UINT kUNIT_VERSION = 6;

const UINT NUM_MB_UNITS = 2;
const UINT NUM_PHASOR_UNITS = 2;

#define SS_YAML_KEY_MB_UNIT "Unit"
#define SS_YAML_KEY_SY6522 "SY6522"
#define SS_YAML_KEY_SY6522_REG_ORB "ORB"
#define SS_YAML_KEY_SY6522_REG_ORA "ORA"
#define SS_YAML_KEY_SY6522_REG_DDRB "DDRB"
#define SS_YAML_KEY_SY6522_REG_DDRA "DDRA"
#define SS_YAML_KEY_SY6522_REG_T1_COUNTER "Timer1 Counter"
#define SS_YAML_KEY_SY6522_REG_T1_LATCH "Timer1 Latch"
#define SS_YAML_KEY_SY6522_REG_T2_COUNTER "Timer2 Counter"
#define SS_YAML_KEY_SY6522_REG_T2_LATCH "Timer2 Latch"
#define SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT "Serial Shift"
#define SS_YAML_KEY_SY6522_REG_ACR "ACR"
#define SS_YAML_KEY_SY6522_REG_PCR "PCR"
#define SS_YAML_KEY_SY6522_REG_IFR "IFR"
#define SS_YAML_KEY_SY6522_REG_IER "IER"
#define SS_YAML_KEY_SSI263 "SSI263"
#define SS_YAML_KEY_SSI263_REG_DUR_PHON "Duration / Phoneme"
#define SS_YAML_KEY_SSI263_REG_INF "Inflection"
#define SS_YAML_KEY_SSI263_REG_RATE_INF "Rate / Inflection"
#define SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP "Control / Articulation / Amplitude"
#define SS_YAML_KEY_SSI263_REG_FILTER_FREQ "Filter Frequency"
#define SS_YAML_KEY_SSI263_REG_CURRENT_MODE "Current Mode"
#define SS_YAML_KEY_AY_CURR_REG "AY Current Register"
#define SS_YAML_KEY_MB_UNIT_STATE "Unit State"
#define SS_YAML_KEY_MB_UNIT_STATE_B "Unit State-B"	// Phasor only
#define SS_YAML_KEY_TIMER1_IRQ "Timer1 IRQ Pending"
#define SS_YAML_KEY_TIMER2_IRQ "Timer2 IRQ Pending"
#define SS_YAML_KEY_SPEECH_IRQ "Speech IRQ Pending"
#define SS_YAML_KEY_TIMER1_ACTIVE "Timer1 Active"
#define SS_YAML_KEY_TIMER2_ACTIVE "Timer2 Active"
#define SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY "Timer1 IRQ Delay"
#define SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY "Timer2 IRQ Delay"

#define SS_YAML_KEY_PHASOR_UNIT "Unit"
#define SS_YAML_KEY_PHASOR_CLOCK_SCALE_FACTOR "Clock Scale Factor"	// Redundant from v6
#define SS_YAML_KEY_PHASOR_MODE "Mode"

#define SS_YAML_KEY_VOTRAX_PHONEME "Votrax Phoneme"

std::string MB_GetSnapshotCardName(void)
{
	static const std::string name("Mockingboard C");
	return name;
}

std::string Phasor_GetSnapshotCardName(void)
{
	static const std::string name("Phasor");
	return name;
}

static void SaveSnapshotSY6522(YamlSaveHelper& yamlSaveHelper, SY6522& sy6522)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SY6522);

	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ORB, sy6522.ORB);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ORA, sy6522.ORA);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_DDRB, sy6522.DDRB);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_DDRA, sy6522.DDRA);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T1_COUNTER, sy6522.TIMER1_COUNTER.w);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T1_LATCH,   sy6522.TIMER1_LATCH.w);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY,    sy6522.timer1IrqDelay);	// v4
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T2_COUNTER, sy6522.TIMER2_COUNTER.w);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SY6522_REG_T2_LATCH,   sy6522.TIMER2_LATCH.w);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY,    sy6522.timer2IrqDelay);	// v4
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT, sy6522.SERIAL_SHIFT);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_ACR, sy6522.ACR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_PCR, sy6522.PCR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_IFR, sy6522.IFR);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SY6522_REG_IER, sy6522.IER);
	// NB. No need to write ORA_NO_HS, since same data as ORA, just without handshake
}

static void SaveSnapshotSSI263(YamlSaveHelper& yamlSaveHelper, SSI263A& ssi263)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SSI263);

	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_DUR_PHON, ssi263.DurationPhoneme);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_INF, ssi263.Inflection);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_RATE_INF, ssi263.RateInflection);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP, ssi263.CtrlArtAmp);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_FILTER_FREQ, ssi263.FilterFreq);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_CURRENT_MODE, ssi263.CurrentMode);
}

void MB_SaveSnapshot(YamlSaveHelper& yamlSaveHelper, const UINT uSlot)
{
	const UINT nMbCardNum = uSlot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	YamlSaveHelper::Slot slot(yamlSaveHelper, MB_GetSnapshotCardName(), uSlot, kUNIT_VERSION);	// fixme: object should be just 1 Mockingboard card & it will know its slot

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, g_bVotraxPhoneme);

	for(UINT i=0; i<NUM_MB_UNITS; i++)
	{
		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_MB_UNIT, i);

		SaveSnapshotSY6522(yamlSaveHelper, pMB->sy6522);
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum, std::string(""));
		SaveSnapshotSSI263(yamlSaveHelper, pMB->SpeechChip);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_TIMER1_IRQ, "false");
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_TIMER2_IRQ, "false");
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_SPEECH_IRQ, "false");
		yamlSaveHelper.SaveBool(SS_YAML_KEY_TIMER1_ACTIVE, pMB->bTimer1Active);
		yamlSaveHelper.SaveBool(SS_YAML_KEY_TIMER2_ACTIVE, pMB->bTimer2Active);

		nDeviceNum++;
		pMB++;
	}
}

static void LoadSnapshotSY6522(YamlLoadHelper& yamlLoadHelper, SY6522& sy6522, UINT version)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_SY6522))
		throw std::string("Card: Expected key: ") + std::string(SS_YAML_KEY_SY6522);

	sy6522.ORB  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ORB);
	sy6522.ORA  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ORA);
	sy6522.DDRB = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_DDRB);
	sy6522.DDRA = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_DDRA);
	sy6522.TIMER1_COUNTER.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T1_COUNTER);
	sy6522.TIMER1_LATCH.w   = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T1_LATCH);
	sy6522.TIMER2_COUNTER.w = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T2_COUNTER);
	sy6522.TIMER2_LATCH.w   = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_T2_LATCH);
	sy6522.SERIAL_SHIFT     = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_SERIAL_SHIFT);
	sy6522.ACR  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_ACR);
	sy6522.PCR  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_PCR);
	sy6522.IFR  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_IFR);
	sy6522.IER  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_REG_IER);
	sy6522.ORA_NO_HS = 0;	// Not saved

	sy6522.timer1IrqDelay = sy6522.timer2IrqDelay = 0;

	if (version >= 4)
	{
		sy6522.timer1IrqDelay = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_TIMER1_IRQ_DELAY);
		sy6522.timer2IrqDelay = yamlLoadHelper.LoadUint(SS_YAML_KEY_SY6522_TIMER2_IRQ_DELAY);
	}

	yamlLoadHelper.PopMap();
}

static void LoadSnapshotSSI263(YamlLoadHelper& yamlLoadHelper, SSI263A& ssi263)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_SSI263))
		throw std::string("Card: Expected key: ") + std::string(SS_YAML_KEY_SSI263);

	ssi263.DurationPhoneme = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_DUR_PHON);
	ssi263.Inflection      = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_INF);
	ssi263.RateInflection  = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_RATE_INF);
	ssi263.CtrlArtAmp      = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP);
	ssi263.FilterFreq      = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_FILTER_FREQ);
	ssi263.CurrentMode     = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_CURRENT_MODE);

	yamlLoadHelper.PopMap();
}

bool MB_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != 4 && slot != 5)	// fixme
		throw std::string("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	g_bVotraxPhoneme = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;

	AY8910UpdateSetCycles();

	const UINT nMbCardNum = slot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;

	for(UINT i=0; i<NUM_MB_UNITS; i++)
	{
		char szNum[2] = {char('0' + i), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::string("Card: Expected key: ") + std::string(unit);

		LoadSnapshotSY6522(yamlLoadHelper, pMB->sy6522, version);
		UpdateIFR(pMB, 0, pMB->sy6522.IFR);					// Assert any pending IRQs (GH#677)
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum, std::string(""));
		LoadSnapshotSSI263(yamlLoadHelper, pMB->SpeechChip);

		pMB->nAYCurrentRegister = yamlLoadHelper.LoadUint(SS_YAML_KEY_AY_CURR_REG);
		yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_IRQ);	// Consume
		yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_IRQ);	// Consume
		yamlLoadHelper.LoadBool(SS_YAML_KEY_SPEECH_IRQ);	// Consume

		if (version >= 2)
		{
			pMB->bTimer1Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_ACTIVE);
			pMB->bTimer2Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_ACTIVE);
		}

		pMB->state = AY_INACTIVE;
		pMB->stateB = AY_INACTIVE;
		if (version >= 3)
			pMB->state = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE) & 7);

		yamlLoadHelper.PopMap();

		//

		if (version == 1)
		{
			StartTimer1_LoadStateV1(pMB);	// Attempt to start timer
		}
		else	// version >= 2
		{
			if (pMB->bTimer1Active)
				StartTimer1(pMB);			// Attempt to start timer
		}

		if (pMB->bTimer1Active)
		{
			const UINT id = nDeviceNum*kNumTimersPer6522+0;	// TIMER1
			SyncEvent* pSyncEvent = g_syncEvent[id];
			pSyncEvent->SetCycles(pMB->sy6522.TIMER1_COUNTER.w + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
			g_SynchronousEventMgr.Insert(pSyncEvent);
		}
		if (pMB->bTimer2Active)
		{
			const UINT id = nDeviceNum*kNumTimersPer6522+1;	// TIMER2
			SyncEvent* pSyncEvent = g_syncEvent[id];
			pSyncEvent->SetCycles(pMB->sy6522.TIMER2_COUNTER.w + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
			g_SynchronousEventMgr.Insert(pSyncEvent);
		}

		// FIXME: currently only support a single speech chip
		// NB. g_bVotraxPhoneme is never true, as the phoneme playback completes in SSI263Thread() before this point in the save-state.
		// NB. SpeechChip.DurationPhoneme will mostly be non-zero during speech playback, as this is the SSI263 register, not whether the phonene is active.
		// FIXME: So possible race-condition between saving-state & SSI263Thread()
		if (pMB->SpeechChip.DurationPhoneme || g_bVotraxPhoneme)
		{
			g_nSSI263Device = nDeviceNum;
			g_bPhasorEnable = false;
			SetSpeechIRQ(pMB);
		}

		nDeviceNum++;
		pMB++;
	}

	AY8910_InitClock((int)Get6502BaseClock());

	// NB. g_SoundcardType & g_bPhasorEnable setup in MB_InitializeIO() -> MB_SetSoundcardType()

	return true;
}

void Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper, const UINT uSlot)
{
	if (uSlot != 4)
		throw std::string("Card: Phasor only supported in slot-4");

	UINT nDeviceNum = 0;
	SY6522_AY8910* pMB = &g_MB[0];	// fixme: Phasor uses MB's slot4(2x6522), slot4(2xSSI263), but slot4+5(4xAY8910)

	YamlSaveHelper::Slot slot(yamlSaveHelper, Phasor_GetSnapshotCardName(), uSlot, kUNIT_VERSION);	// fixme: object should be just 1 Mockingboard card & it will know its slot

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveUint(SS_YAML_KEY_PHASOR_MODE, g_phasorMode);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, g_bVotraxPhoneme);

	for(UINT i=0; i<NUM_PHASOR_UNITS; i++)
	{
		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_PHASOR_UNIT, i);

		SaveSnapshotSY6522(yamlSaveHelper, pMB->sy6522);
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum+0, std::string("-A"));
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum+1, std::string("-B"));
		SaveSnapshotSSI263(yamlSaveHelper, pMB->SpeechChip);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE_B, pMB->stateB);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_TIMER1_IRQ, "false");
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_TIMER2_IRQ, "false");
		yamlSaveHelper.Save("%s: %s # Not supported\n", SS_YAML_KEY_SPEECH_IRQ, "false");
		yamlSaveHelper.SaveBool(SS_YAML_KEY_TIMER1_ACTIVE, pMB->bTimer1Active);
		yamlSaveHelper.SaveBool(SS_YAML_KEY_TIMER2_ACTIVE, pMB->bTimer2Active);

		nDeviceNum += 2;
		pMB++;
	}
}

bool Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != 4)	// fixme
		throw std::string("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	if (version < 6)
		yamlLoadHelper.LoadUint(SS_YAML_KEY_PHASOR_CLOCK_SCALE_FACTOR);	// Consume redundant data

	UINT phasorMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_PHASOR_MODE);
	if (version < 6)
	{
		if (phasorMode == 0)
			phasorMode = PH_Mockingboard;
		else
			phasorMode = PH_Phasor;
	}
	g_phasorMode = (PHASOR_MODE) phasorMode;
	g_PhasorClockScaleFactor = (g_phasorMode == PH_Phasor) ? 2 : 1;

	g_bVotraxPhoneme = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;

	AY8910UpdateSetCycles();

	UINT nDeviceNum = 0;
	SY6522_AY8910* pMB = &g_MB[0];

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;

	for(UINT i=0; i<NUM_PHASOR_UNITS; i++)
	{
		char szNum[2] = {char('0' + i), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::string("Card: Expected key: ") + std::string(unit);

		LoadSnapshotSY6522(yamlLoadHelper, pMB->sy6522, version);
		UpdateIFR(pMB, 0, pMB->sy6522.IFR);					// Assert any pending IRQs (GH#677)
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum+0, std::string("-A"));
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum+1, std::string("-B"));
		LoadSnapshotSSI263(yamlLoadHelper, pMB->SpeechChip);

		pMB->nAYCurrentRegister = yamlLoadHelper.LoadUint(SS_YAML_KEY_AY_CURR_REG);
		yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_IRQ);	// Consume
		yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_IRQ);	// Consume
		yamlLoadHelper.LoadBool(SS_YAML_KEY_SPEECH_IRQ);	// Consume

		if (version >= 2)
		{
			pMB->bTimer1Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_ACTIVE);
			pMB->bTimer2Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_ACTIVE);
		}

		pMB->state = AY_INACTIVE;
		pMB->stateB = AY_INACTIVE;
		if (version >= 3)
			pMB->state = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE) & 7);
		if (version >= 5)
			pMB->stateB = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE_B) & 7);

		yamlLoadHelper.PopMap();

		//

		if (version == 1)
		{
			StartTimer1_LoadStateV1(pMB);	// Attempt to start timer
		}
		else	// version >= 2
		{
			if (pMB->bTimer1Active)
				StartTimer1(pMB);			// Attempt to start timer
		}

		if (pMB->bTimer1Active)
		{
			const UINT id = (nDeviceNum/2)*kNumTimersPer6522+0;	// TIMER1
			SyncEvent* pSyncEvent = g_syncEvent[id];
			pSyncEvent->SetCycles(pMB->sy6522.TIMER1_COUNTER.w + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
			g_SynchronousEventMgr.Insert(pSyncEvent);
		}
		if (pMB->bTimer2Active)
		{
			const UINT id = (nDeviceNum/2)*kNumTimersPer6522+1;	// TIMER2
			SyncEvent* pSyncEvent = g_syncEvent[id];
			pSyncEvent->SetCycles(pMB->sy6522.TIMER2_COUNTER.w + kExtraTimerCycles);	// NB. use COUNTER, not LATCH
			g_SynchronousEventMgr.Insert(pSyncEvent);
		}

		// FIXME: currently only support a single speech chip
		if (pMB->SpeechChip.DurationPhoneme || g_bVotraxPhoneme)
		{
			g_nSSI263Device = nDeviceNum+1;	// +1 as speech is always 2nd device of the pair
			g_bPhasorEnable = true;
			SetSpeechIRQ(pMB);
		}

		nDeviceNum += 2;
		pMB++;
	}

	AY8910_InitClock((int)(Get6502BaseClock() * g_PhasorClockScaleFactor));

	// NB. g_SoundcardType & g_bPhasorEnable setup in MB_InitializeIO() -> MB_SetSoundcardType()

	return true;
}
