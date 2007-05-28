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

#define LOG_SSI263 0

#include "StdAfx.h"
#pragma  hdrstop
#include <wchar.h>

#include "ay8910.h"
#include "SSI263Phonemes.h"


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

#define Phasor_SY6522A_CS		4
#define Phasor_SY6522B_CS		7
#define Phasor_SY6522A_Offset	(1<<Phasor_SY6522A_CS)
#define Phasor_SY6522B_Offset	(1<<Phasor_SY6522B_CS)

typedef struct
{
	SY6522 sy6522;
	BYTE nAY8910Number;
	BYTE nAYCurrentRegister;
	BYTE nTimerStatus;
	SSI263A SpeechChip;
} SY6522_AY8910;


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

// Timer vars
static ULONG g_n6522TimerPeriod = 0;
static USHORT g_nMBTimerDevice = 0;	// SY6522 device# which is generating timer IRQ

// SSI263 vars:
static USHORT g_nSSI263Device = 0;	// SSI263 device# which is generating phoneme-complete IRQ
static int g_nCurrentActivePhoneme = -1;
static bool g_bStopPhoneme = false;
static bool g_bVotraxPhoneme = false;

static const DWORD SAMPLE_RATE = 44100;	// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample

static short* ppAYVoiceBuffer[NUM_VOICES] = {0};

static unsigned __int64	g_nMB_InActiveCycleCount = 0;
static bool g_bMB_RegAccessedFlag = false;
static bool g_bMB_Active = true;

static HANDLE g_hThread = NULL;

static bool g_bMBAvailable = false;

//

static eSOUNDCARDTYPE g_SoundcardType = SC_MOCKINGBOARD;	// Mockingboard enable (dialog var)
static bool g_bPhasorEnable = false;
static BYTE g_nPhasorMode = 0;	// 0=Mockingboard emulation, 1=Phasor native

//-------------------------------------

static const unsigned short g_nMB_NumChannels = 2;

static const DWORD g_dwDSBufferSize = 16 * 1024 * sizeof(short) * g_nMB_NumChannels;

static const SHORT nWaveDataMin = (SHORT)0x8000;
static const SHORT nWaveDataMax = (SHORT)0x7FFF;

static short g_nMixBuffer[g_dwDSBufferSize / sizeof(short)];


static VOICE MockingboardVoice = {0};
static VOICE SSI263Voice[64] = {0};

static const int g_nNumEvents = 2;
static HANDLE g_hSSI263Event[g_nNumEvents] = {NULL};	// 1: Phoneme finished playing, 2: Exit thread
static DWORD g_dwMaxPhonemeLen = 0;

// When 6522 IRQ is *not* active use 60Hz update freq for MB voices
static const double g_f6522TimerPeriod_NoIRQ = CLK_6502 / 60.0;		// Constant whatever the CLK is set to

//---------------------------------------------------------------------------

// External global vars:
bool g_bMBTimerIrqActive = false;
UINT32 g_uTimer1IrqCount = 0;	// DEBUG

//---------------------------------------------------------------------------

// Forward refs:
static DWORD WINAPI SSI263Thread(LPVOID);
static void Votrax_Write(BYTE nDevice, BYTE nValue);

//---------------------------------------------------------------------------

static void StartTimer(SY6522_AY8910* pMB)
{
	if((pMB->nAY8910Number & 1) != SY6522_DEVICE_A)
		return;

	if((pMB->sy6522.IER & IxR_TIMER1) == 0x00)
		return;

	USHORT nPeriod = pMB->sy6522.TIMER1_LATCH.w;

	if(nPeriod <= 0xff)		// Timer1L value has been written (but TIMER1H hasn't)
		return;

	pMB->nTimerStatus = 1;

	// 6522 CLK runs at same speed as 6502 CLK
	g_n6522TimerPeriod = nPeriod;

	g_bMBTimerIrqActive = true;
	g_nMBTimerDevice = pMB->nAY8910Number;
}

//-----------------------------------------------------------------------------

static void StopTimer(SY6522_AY8910* pMB)
{
	pMB->nTimerStatus = 0;
	g_bMBTimerIrqActive = false;
	g_nMBTimerDevice = 0;
}

//-----------------------------------------------------------------------------

static void ResetSY6522(SY6522_AY8910* pMB)
{
	memset(&pMB->sy6522,0,sizeof(SY6522));

	if(pMB->nTimerStatus)
		StopTimer(pMB);

	pMB->nAYCurrentRegister = 0;
}

//-----------------------------------------------------------------------------

static void AY8910_Write(BYTE nDevice, BYTE nReg, BYTE nValue, BYTE nAYDevice)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	if((nValue & 4) == 0)
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

		int nAYFunc = (nBDIR<<2) | (nBC2<<1) | nBC1;
		enum {AY_NOP0, AY_NOP1, AY_INACTIVE, AY_READ, AY_NOP4, AY_NOP5, AY_WRITE, AY_LATCH};

		switch(nAYFunc)
		{
			case AY_INACTIVE:	// 4: INACTIVE
				break;

			case AY_READ:		// 5: READ FROM PSG (need to set DDRA to input)
				break;

			case AY_WRITE:		// 6: WRITE TO PSG
				_AYWriteReg(nDevice+2*nAYDevice, pMB->nAYCurrentRegister, pMB->sy6522.ORA);
				break;

			case AY_LATCH:		// 7: LATCH ADDRESS
				if(pMB->sy6522.ORA <= 0x0F)
					pMB->nAYCurrentRegister = pMB->sy6522.ORA & 0x0F;
				// else Pro-Mockingboard (clone from HK)
				break;
		}
	}
}

static void UpdateIFR(SY6522_AY8910* pMB)
{
	pMB->sy6522.IFR &= 0x7F;

	if(pMB->sy6522.IFR & pMB->sy6522.IER & 0x7F)
		pMB->sy6522.IFR |= 0x80;

	// Now update the IRQ signal from all 6522s
	// . OR-sum of all active TIMER1, TIMER2 & SPEECH sources (from all 6522s)
	UINT bIRQ = 0;
	for(UINT i=0; i<NUM_SY6522; i++)
		bIRQ |= g_MB[i].sy6522.IFR & 0x80;

	// NB. Mockingboard generates IRQ on both 6522s:
	// . SSI263's IRQ (A/!R) is routed via the 2nd 6522 (at $Cx80) and must generate a 6502 IRQ (not NMI)
	// . SC-01's IRQ (A/!R) is also routed via a (2nd?) 6522
	// Phasor's SSI263 appears to be wired directly to the 6502's IRQ (ie. not via a 6522)
	// . I assume Phasor's 6522s just generate 6502 IRQs (not NMIs)

	if (bIRQ)
	{
	    CpuIrqAssert(IS_6522);
	}
	else
	{
	    CpuIrqDeassert(IS_6522);
	}
}

static void SY6522_Write(BYTE nDevice, BYTE nReg, BYTE nValue)
{
	g_bMB_RegAccessedFlag = true;
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
					int nAY_CS = (g_nPhasorMode & 1) ? (~(nValue >> 3) & 3) : 1;

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
			/* Initiates timer1 & clears time-out of timer1 */

			// Clear Timer Interrupt Flag.
			pMB->sy6522.IFR &= ~IxR_TIMER1;
			UpdateIFR(pMB);

			pMB->sy6522.TIMER1_LATCH.h = nValue;
			pMB->sy6522.TIMER1_COUNTER.w = pMB->sy6522.TIMER1_LATCH.w;

			StartTimer(pMB);
			break;
		case 0x07:	// TIMER1H_LATCH
			// Clear Timer1 Interrupt Flag.
			pMB->sy6522.TIMER1_LATCH.h = nValue;
			pMB->sy6522.IFR &= ~IxR_TIMER1;
			UpdateIFR(pMB);
			break;
		case 0x08:	// TIMER2L
			pMB->sy6522.TIMER2_LATCH.l = nValue;
			break;
		case 0x09:	// TIMER2H
			// Clear Timer2 Interrupt Flag.
			pMB->sy6522.IFR &= ~IxR_TIMER2;
			UpdateIFR(pMB);

			pMB->sy6522.TIMER2_LATCH.h = nValue;
			pMB->sy6522.TIMER2_COUNTER.w = pMB->sy6522.TIMER2_LATCH.w;
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
			nValue |= 0x80;	// Set high bit
			nValue ^= 0x7F;	// Make mask
			pMB->sy6522.IFR &= nValue;
			UpdateIFR(pMB);
			break;
		case 0x0e:	// IER
			if(!(nValue & 0x80))
			{
				// Clear those bits which are set in the lower 7 bits.
				nValue ^= 0x7F;
				pMB->sy6522.IER &= nValue;
				UpdateIFR(pMB);
				
				// Check if timer has been disabled.
				if(pMB->sy6522.IER & IxR_TIMER1)
					break;

				if(pMB->nTimerStatus == 0)
					break;
				
				pMB->nTimerStatus = 0;
				
				// Stop timer
				StopTimer(pMB);
			}
			else
			{
				// Set those bits which are set in the lower 7 bits.
				nValue &= 0x7F;
				pMB->sy6522.IER |= nValue;
				UpdateIFR(pMB);
				StartTimer(pMB);
			}
			break;
		case 0x0f:	// ORA_NO_HS
			break;
	}
}

//-----------------------------------------------------------------------------

static BYTE SY6522_Read(BYTE nDevice, BYTE nReg)
{
	g_bMB_RegAccessedFlag = true;
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
			nValue = pMB->sy6522.TIMER1_COUNTER.l;
			pMB->sy6522.IFR &= ~IxR_TIMER1;		// Also clears Timer1 Interrupt Flag
			UpdateIFR(pMB);
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
			pMB->sy6522.IFR &= ~IxR_TIMER2;		// Also clears Timer2 Interrupt Flag
			UpdateIFR(pMB);
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
			nValue = 0x80;
			break;
		case 0x0f:	// ORA_NO_HS
			nValue = pMB->sy6522.ORA;
			break;
	}

	return nValue;
}

//---------------------------------------------------------------------------

void SSI263_Play(unsigned int nPhoneme);

#if 0
typedef struct
{
	BYTE DurationPhonome;
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

static BYTE SSI263_Read(BYTE nDevice, BYTE nReg)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	// Regardless of register, just return inverted A/!R in bit7
	// . A/!R is low for IRQ

	return pMB->SpeechChip.CurrentMode << 7;
}

static void SSI263_Write(BYTE nDevice, BYTE nReg, BYTE nValue)
{
	SY6522_AY8910* pMB = &g_MB[nDevice];

	switch(nReg)
	{
	case SSI_DURPHON:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "DUR   = 0x%02X, PHON = 0x%02X\n\n", nValue>>6, nValue&PHONEME_MASK);
#endif

		// Datasheet is not clear, but a write to DURPHON must clear the IRQ
		if(g_bPhasorEnable)
		{
		    CpuIrqDeassert(IS_SPEECH);
		}
		else
		{
			pMB->sy6522.IFR &= ~IxR_PERIPHERAL;
			UpdateIFR(pMB);
		}
		pMB->SpeechChip.CurrentMode &= ~1;	// Clear SSI263's D7 pin

		pMB->SpeechChip.DurationPhonome = nValue;

		g_nSSI263Device = nDevice;

		// Phoneme output not dependent on CONTROL bit
		if(g_bPhasorEnable)
		{
			if(nValue || (g_nCurrentActivePhoneme<0))
				SSI263_Play(nValue & PHONEME_MASK);
		}
		else
		{
			SSI263_Play(nValue & PHONEME_MASK);
		}
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
#endif
		if((pMB->SpeechChip.CtrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK))	// H->L
			pMB->SpeechChip.CurrentMode = pMB->SpeechChip.DurationPhonome & DURATION_MODE_MASK;
		pMB->SpeechChip.CtrlArtAmp = nValue;
		break;
	case SSI_FILFREQ:
#if LOG_SSI263
		if(g_fh) fprintf(g_fh, "FFREQ = 0x%02X\n", nValue);
#endif
		pMB->SpeechChip.FilterFreq = nValue;
		break;
	default:
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
	pMB->sy6522.IFR &= ~IxR_VOTRAX;
	UpdateIFR(pMB);

	g_nSSI263Device = nDevice;

	SSI263_Play(Votrax2SSI263[nValue & PHONEME_MASK]);
}

//===========================================================================

static void MB_Update()
{
	if(!MockingboardVoice.bActive)
		return;

	//

	if(!g_bMB_RegAccessedFlag)
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

	static DWORD dwByteOffset = (DWORD)-1;
	static int nNumSamplesError = 0;


	int nNumSamples;
	double n6522TimerPeriod = MB_GetFramePeriod();

	double nIrqFreq = g_fCurrentCLK6502 / n6522TimerPeriod + 0.5;			// Round-up
	int nNumSamplesPerPeriod = (int) ((double)SAMPLE_RATE / nIrqFreq);		// Eg. For 60Hz this is 735
	nNumSamples = nNumSamplesPerPeriod + nNumSamplesError;					// Apply correction
	if(nNumSamples <= 0)
		nNumSamples = 0;
	if(nNumSamples > 2*nNumSamplesPerPeriod)
		nNumSamples = 2*nNumSamplesPerPeriod;

	if(nNumSamples)
		for(int nChip=0; nChip<NUM_AY8910; nChip++)
			AY8910Update(nChip, &ppAYVoiceBuffer[nChip*NUM_VOICES_PER_AY8910], nNumSamples);

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	HRESULT hr = MockingboardVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if(FAILED(hr))
		return;

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
				dwByteOffset = dwCurrentWriteCursor;
		}
		else
		{
			// |xxW----------Pxxx|
			if((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
				dwByteOffset = dwCurrentWriteCursor;
		}
	}

	int nBytesRemaining = dwByteOffset - dwCurrentPlayCursor;
	if(nBytesRemaining < 0)
		nBytesRemaining += g_dwDSBufferSize;

	// Calc correction factor so that play-buffer doesn't under/overflow
	if(nBytesRemaining < g_dwDSBufferSize / 4)
		nNumSamplesError++;							// < 0.25 of buffer remaining
	else if(nBytesRemaining > g_dwDSBufferSize / 2)
		nNumSamplesError--;							// > 0.50 of buffer remaining
	else
		nNumSamplesError = 0;						// Acceptable amount of data in buffer

	if(nNumSamples == 0)
		return;

	//

	double fAttenuation = g_bPhasorEnable ? 2.0/3.0 : 1.0;

	for(int i=0; i<nNumSamples; i++)
	{
		// Mockingboard stereo (all voices on an AY8910 wire-or'ed together)
		// L = Address.b7=0, R = Address.b7=1
		int nDataL = 0, nDataR = 0;

		for(unsigned int j=0; j<NUM_VOICES_PER_AY8910; j++)
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

//-----------------------------------------------------------------------------

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

		SSI263Voice[g_nCurrentActivePhoneme].bActive = false;
		g_nCurrentActivePhoneme = -1;

		// Phoneme complete, so generate IRQ if necessary
		SY6522_AY8910* pMB = &g_MB[g_nSSI263Device];

		if(g_bPhasorEnable)
		{
			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED))
			{
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin

				// Phasor's SSI263.IRQ line appears to be wired directly to IRQ (Bypassing the 6522)
				CpuIrqAssert(IS_SPEECH);
			}
		}
		else
		{
			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED) && (pMB->sy6522.PCR == 0x0C))
			{
				pMB->sy6522.IFR |= IxR_PERIPHERAL;
				UpdateIFR(pMB);
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin
			}
		}

		//

		if(g_bVotraxPhoneme && (pMB->sy6522.PCR == 0xB0))
		{
			// !A/R: Time-out of old phoneme (signal goes from low to high)

			pMB->sy6522.IFR |= IxR_VOTRAX;
			UpdateIFR(pMB);

			g_bVotraxPhoneme = false;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------

static void SSI263_Play(unsigned int nPhoneme)
{
#if 1
	HRESULT hr;

	if(g_nCurrentActivePhoneme >= 0)
	{
		// A write to DURPHON before previous phoneme has completed
		g_bStopPhoneme = true;
		hr = SSI263Voice[g_nCurrentActivePhoneme].lpDSBvoice->Stop();
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
	//
	// Create single Mockingboard voice
	//

	DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	SHORT* pDSLockedBuffer;

	if(!g_bDSAvailable)
		return false;

	HRESULT hr = DSGetSoundBuffer(&MockingboardVoice, DSBCAPS_CTRLVOLUME, g_dwDSBufferSize, SAMPLE_RATE, 2);
	if(FAILED(hr))
	{
		if(g_fh) fprintf(g_fh, "MB: DSGetSoundBuffer failed (%08X)\n",hr);
		return false;
	}

	if(!DSZeroVoiceBuffer(&MockingboardVoice, "MB", g_dwDSBufferSize))
		return false;

	MockingboardVoice.bActive = true;

	// Volume might've been setup from value in Registry
	if(!MockingboardVoice.nVolume)
		MockingboardVoice.nVolume = DSBVOLUME_MAX;

	MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);

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

	g_hSSI263Event[1] = CreateEvent(NULL,	// lpEventAttributes
									FALSE,	// bManualReset (FALSE = auto-reset)
									FALSE,	// bInitialState (FALSE = non-signaled)
									NULL);	// lpName

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
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: DSGetSoundBuffer failed (%08X)\n",hr);
			return false;
		}

		hr = DSGetLock(SSI263Voice[i].lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0);
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
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: SetNotifyPos failed (%08X)\n",hr);
			return false;
		}

		hr = SSI263Voice[i].lpDSBvoice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
		if(FAILED(hr))
		{
			if(g_fh) fprintf(g_fh, "SSI263: DSUnlock failed (%08X)\n",hr);
			return false;
		}

		SSI263Voice[i].bActive = false;
		SSI263Voice[i].nVolume = MockingboardVoice.nVolume;		// Use same volume as MB
		SSI263Voice[i].lpDSBvoice->SetVolume(SSI263Voice[i].nVolume);
	}

	//

	DWORD dwThreadId;

	g_hThread = CreateThread(NULL,				// lpThreadAttributes
								0,				// dwStackSize
								SSI263Thread,
								NULL,			// lpParameter
								0,				// dwCreationFlags : 0 = Run immediately
								&dwThreadId);	// lpThreadId

	SetThreadPriority(g_hThread, THREAD_PRIORITY_TIME_CRITICAL);

	return true;
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

static BYTE __stdcall PhasorIO (WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);
static BYTE __stdcall MB_Read(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);
static BYTE __stdcall MB_Write(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);

void MB_Initialize()
{
	if(g_bDisableDirectSound)
	{
		MockingboardVoice.bMute = true;
		g_SoundcardType = SC_NONE;
	}
	else
	{
		memset(&g_MB,0,sizeof(g_MB));

		int i;
		for(i=0; i<NUM_VOICES; i++)
			ppAYVoiceBuffer[i] = new short [SAMPLE_RATE];	// Buffer can hold a max of 1 seconds worth of samples

		AY8910_InitAll((int)g_fCurrentCLK6502, SAMPLE_RATE);

		for(i=0; i<NUM_AY8910; i++)
			g_MB[i].nAY8910Number = i;

		//

		//DSInit();
		g_bMBAvailable = MB_DSInit();

		MB_Reset();
	}

	//

	g_bMB_Active = (g_SoundcardType != SC_NONE);

	//

	const UINT uSlot4 = 4;
	RegisterIoHandler(uSlot4, PhasorIO, PhasorIO, MB_Read, MB_Write, NULL, NULL);

	const UINT uSlot5 = 5;
	RegisterIoHandler(uSlot5, PhasorIO, PhasorIO, MB_Read, MB_Write, NULL, NULL);
}

//-----------------------------------------------------------------------------

// NB. Called when /g_fCurrentCLK6502/ changes
void MB_Reinitialize()
{
	AY8910_InitClock((int)g_fCurrentCLK6502);
}

//-----------------------------------------------------------------------------

void MB_Destroy()
{
	MB_DSUninit();
	//DSUninit();

	for(int i=0; i<NUM_VOICES; i++)
		delete [] ppAYVoiceBuffer[i];
}

//-----------------------------------------------------------------------------

void MB_Reset()
{
	if(!g_bDSAvailable)
		return;

	for(int i=0; i<NUM_AY8910; i++)
	{
		ResetSY6522(&g_MB[i]);
		AY8910_reset(i);
	}

	g_nPhasorMode = 0;
	MB_Reinitialize();	// Reset CLK for AY8910s
}

//-----------------------------------------------------------------------------

static BYTE __stdcall MB_Read(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	CpuCalcCycles(nCyclesLeft);

	if(!IS_APPLE2 && !MemCheckSLOTCXROM())
		return mem[nAddr];

	if(g_SoundcardType == SC_NONE)
		return 0;

	BYTE nMB = (nAddr>>8)&0xf - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
			return 0;

		BYTE nRes = 0;
		int CS;

		if(g_nPhasorMode & 1)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else															// Mockingboard Mode
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2

		if(CS & 1)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);

		if(CS & 2)
			nRes |= SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);

		if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
			nRes |= SSI263_Read(nMB, nAddr&0xf);

		return nRes;
	}

	if(nOffset <= (SY6522A_Offset+0x0F))
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf);
	else if((nOffset >= SY6522B_Offset) && (nOffset <= (SY6522B_Offset+0x0F)))
		return SY6522_Read(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf);
	else if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
		return SSI263_Read(nMB, nAddr&0xf);
	else
		return 0;
}

//-----------------------------------------------------------------------------

static BYTE __stdcall MB_Write(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	CpuCalcCycles(nCyclesLeft);

	if(!IS_APPLE2 && !MemCheckSLOTCXROM())
		return 0;

	if(g_SoundcardType == SC_NONE)
		return 0;

	BYTE nMB = (nAddr>>8)&0xf - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if(g_bPhasorEnable)
	{
		if(nMB != 0)	// Slot4 only
			return 0;

		int CS;

		if(g_nPhasorMode & 1)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else															// Mockingboard Mode
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2

		if(CS & 1)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);

		if(CS & 2)
			SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);

		if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
			SSI263_Write(nMB*2+1, nAddr&0xf, nValue);		// Second 6522 is used for speech chip

		return 0;
	}

	if(nOffset <= (SY6522A_Offset+0x0F))
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_A, nAddr&0xf, nValue);
	else if((nOffset >= SY6522B_Offset) && (nOffset <= (SY6522B_Offset+0x0F)))
		SY6522_Write(nMB*NUM_DEVS_PER_MB + SY6522_DEVICE_B, nAddr&0xf, nValue);
	else if((nOffset >= SSI263_Offset) && (nOffset <= (SSI263_Offset+0x05)))
		SSI263_Write(nMB*2+1, nAddr&0xf, nValue);		// Second 6522 is used for speech chip

	return 0;
}

//-----------------------------------------------------------------------------

static BYTE __stdcall PhasorIO (WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	if(!g_bPhasorEnable)
		return 0;

	if(g_nPhasorMode < 2)
		g_nPhasorMode = nAddr & 1;

	double fCLK = (nAddr & 4) ? CLK_6502*2 : CLK_6502;

	AY8910_InitClock((int)fCLK);

	return 0;
}

//-----------------------------------------------------------------------------

void MB_Mute()
{
	if(g_SoundcardType == SC_NONE)
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
	if(g_SoundcardType == SC_NONE)
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

// Called by ContinueExecution() at the end of every video frame
void MB_EndOfFrame()
{
	if(g_SoundcardType == SC_NONE)
		return;

	if(!g_bFullSpeed && !g_bMBTimerIrqActive && !(g_MB[0].sy6522.IFR & IxR_TIMER1))
		MB_Update();
}

//-----------------------------------------------------------------------------

// Called by InternalCpuExecute() after every opcode
// OLD: Called by CpuExecute() & CpuCalcCycles()
void MB_UpdateCycles(USHORT nClocks)
{
	if(g_SoundcardType == SC_NONE)
		return;

	for(int i=0; i<NUM_SY6522; i++)
	{
		SY6522_AY8910* pMB = &g_MB[i];

		USHORT OldTimer1 = pMB->sy6522.TIMER1_COUNTER.w;
		USHORT OldTimer2 = pMB->sy6522.TIMER2_COUNTER.w;

		pMB->sy6522.TIMER1_COUNTER.w -= nClocks;
		pMB->sy6522.TIMER2_COUNTER.w -= nClocks;

		// Check for counter underflow
		bool bTimer1Underflow = (!(OldTimer1 & 0x8000) && (pMB->sy6522.TIMER1_COUNTER.w & 0x8000));
		bool bTimer2Underflow = (!(OldTimer2 & 0x8000) && (pMB->sy6522.TIMER2_COUNTER.w & 0x8000));

		if( bTimer1Underflow && (g_nMBTimerDevice == i) && g_bMBTimerIrqActive )
		{
			g_uTimer1IrqCount++;	// DEBUG

			pMB->sy6522.IFR |= IxR_TIMER1;
			UpdateIFR(pMB);

			if((pMB->sy6522.ACR & RUNMODE) == RM_ONESHOT)
			{
				// One-shot mode
				StopTimer(pMB);		// Phasor's playback code uses one-shot mode
			}
			else
			{
				// Free-running mode
				// - Ultima4/5 change ACCESS_TIMER1 after a couple of IRQs into tune
				pMB->sy6522.TIMER1_COUNTER.w = pMB->sy6522.TIMER1_LATCH.w;
				StartTimer(pMB);
			}

			if(!g_bFullSpeed)
				MB_Update();
		}
	}
}

//-----------------------------------------------------------------------------

eSOUNDCARDTYPE MB_GetSoundcardType()
{
	return g_SoundcardType;
}

void MB_SetSoundcardType(eSOUNDCARDTYPE NewSoundcardType)
{
	if ((NewSoundcardType == SC_UNINIT) || (g_SoundcardType == NewSoundcardType))
		return;

	g_SoundcardType = NewSoundcardType;

	if(g_SoundcardType == SC_NONE)
		MB_Mute();

	g_bPhasorEnable = (g_SoundcardType == SC_PHASOR);
}

//-----------------------------------------------------------------------------

double MB_GetFramePeriod()
{
	return (g_bMBTimerIrqActive||(g_MB[0].sy6522.IFR & IxR_TIMER1)) ? (double)g_n6522TimerPeriod : g_f6522TimerPeriod_NoIRQ;
}

bool MB_IsActive()
{
	if(!MockingboardVoice.bActive)
		return false;

	// Ignore /g_bMBTimerIrqActive/ as timer's irq handler will access 6522 regs affecting /g_bMB_Active/

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

	if(MockingboardVoice.bActive)
		MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
}

//===========================================================================

DWORD MB_GetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD dwSlot)
{
	pSS->Hdr.UnitHdr.dwLength = sizeof(SS_CARD_DISK2);
	pSS->Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);

	pSS->Hdr.dwSlot = dwSlot;
	pSS->Hdr.dwType = CT_Mockingboard;

	UINT nMbCardNum = dwSlot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	for(UINT i=0; i<MB_UNITS_PER_CARD; i++)
	{
		memcpy(&pSS->Unit[i].RegsSY6522, &pMB->sy6522, sizeof(SY6522));
		memcpy(&pSS->Unit[i].RegsAY8910, AY8910_GetRegsPtr(nDeviceNum), 16);
		memcpy(&pSS->Unit[i].RegsSSI263, &pMB->SpeechChip, sizeof(SSI263A));
		pSS->Unit[i].nAYCurrentRegister = pMB->nAYCurrentRegister;

		nDeviceNum++;
		pMB++;
	}

	return 0;
}

DWORD MB_SetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD /*dwSlot*/)
{
	if(pSS->Hdr.UnitHdr.dwVersion != MAKE_VERSION(1,0,0,0))
		return -1;

	UINT nMbCardNum = pSS->Hdr.dwSlot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	g_nSSI263Device = 0;
	g_nCurrentActivePhoneme = -1;

	for(UINT i=0; i<MB_UNITS_PER_CARD; i++)
	{
		memcpy(&pMB->sy6522, &pSS->Unit[i].RegsSY6522, sizeof(SY6522));
		memcpy(AY8910_GetRegsPtr(nDeviceNum), &pSS->Unit[i].RegsAY8910, 16);
		memcpy(&pMB->SpeechChip, &pSS->Unit[i].RegsSSI263, sizeof(SSI263A));
		pMB->nAYCurrentRegister = pSS->Unit[i].nAYCurrentRegister;

		StartTimer(pMB);	// Attempt to start timer

		//

		// Crude - currently only support a single speech chip
		// FIX THIS:
		// . Speech chip could be Votrax instead
		// . Is this IRQ compatible with Phasor?
		if(pMB->SpeechChip.DurationPhonome)
		{
			g_nSSI263Device = nDeviceNum;

			if((pMB->SpeechChip.CurrentMode != MODE_IRQ_DISABLED) && (pMB->sy6522.PCR == 0x0C) && (pMB->sy6522.IER & IxR_PERIPHERAL))
			{
				pMB->sy6522.IFR |= IxR_PERIPHERAL;
				UpdateIFR(pMB);
				pMB->SpeechChip.CurrentMode |= 1;	// Set SSI263's D7 pin
			}
		}

		nDeviceNum++;
		pMB++;
	}

	return 0;
}
