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
 */

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
#include "6522.h"

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
#include "SSI263.h"

#define DBG_MB_SS_CARD 0	// NB. From UI, select Mockingboard (not Phasor)
//#define DBG_MB_UPDATE

//#define Phasor_SY6522A_CS		4
//#define Phasor_SY6522B_CS		7
//#define Phasor_SY6522A_Offset	(1<<Phasor_SY6522A_CS)
//#define Phasor_SY6522B_Offset	(1<<Phasor_SY6522B_CS)

//---------------------------------------------------------------------------

bool MockingboardCard::IsAnyTimer1Active(void)
{
	bool active = false;
	for (UINT i = 0; i < NUM_AY8913; i++)
		active |= g_MB[i].sy6522.IsTimer1Active();
	return active;
}

//---------------------------------------------------------------------------

#ifdef _DEBUG
void MockingboardCard::Get6522IrqDescription(std::string& desc)
{
	bool isIRQ = false;
	for (UINT i = 0; i < NUM_SY6522; i++)
	{
		if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IFR_IRQ)
		{
			isIRQ = true;
			break;
		}
	}

	if (!isIRQ)
		return;

	//

	desc += "Slot-";
	desc += m_slot;
	desc += ": ";

	for (UINT i=0; i<NUM_SY6522; i++)
	{
		if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IFR_IRQ)
		{
			if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_TIMER1)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "TIMER1 ";
			}
			if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_TIMER2)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "TIMER2 ";
			}
			if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_VOTRAX)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "VOTRAX ";
			}
			if (g_MB[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_SSI263)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "SSI263 ";
			}
		}
	}

	desc += "\n";
}
#endif

//-----------------------------------------------------------------------------

void MockingboardCard::AY8910_Write(BYTE nDevice, BYTE nValue, BYTE nAYDevice)
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
					if (g_bPhasorEnable && g_phasorMode == PH_EchoPlus)
						pMB->sy6522.SetRegORA( 0xff & (pMB->sy6522.GetReg(SY6522::rDDRA) ^ 0xff) );	// Phasor (Echo+ mode) doesn't support reading AY8913s - it just reads 1's for the input bits
					else
						pMB->sy6522.SetRegORA( AYReadReg(nDevice+2*nAYDevice, pMB->nAYCurrentRegister) & (pMB->sy6522.GetReg(SY6522::rDDRA) ^ 0xff) );
					break;

				case AY_WRITE:		// 6: WRITE TO PSG
					_AYWriteReg(nDevice+2*nAYDevice, pMB->nAYCurrentRegister, pMB->sy6522.GetReg(SY6522::rORA));
					break;

				case AY_LATCH:		// 7: LATCH ADDRESS
					// http://www.worldofspectrum.org/forums/showthread.php?t=23327
					// Selecting an unused register number above 0x0f puts the AY into a state where
					// any values written to the data/address bus are ignored, but can be read back
					// within a few tens of thousands of cycles before they decay to zero.
					if (pMB->sy6522.GetReg(SY6522::rORA) <= 0x0F)
						pMB->nAYCurrentRegister = pMB->sy6522.GetReg(SY6522::rORA) & 0x0F;
					// else Pro-Mockingboard (clone from HK)
					break;
			}
		}

		state = nAYFunc;
	}
}

//-----------------------------------------------------------------------------

void MockingboardCard::WriteToORB(BYTE device)
{
	BYTE value = g_MB[device].sy6522.Read(SY6522::rORB);

	if ((device & 1) == 0 && // SC01 only at $Cn00 (not $Cn80)
		g_MB[device].sy6522.Read(SY6522::rPCR) == 0xB0)
	{
		// Votrax speech data
		const BYTE DDRB = g_MB[device].sy6522.Read(SY6522::rDDRB);
		g_MB[device].ssi263.Votrax_Write((value & DDRB) | (DDRB ^ 0xff));	// DDRB's zero bits (inputs) are high impedence, so output as 1 (GH#952)
		return;
	}

#if DBG_MB_SS_CARD
	if ((nDevice & 1) == 1)
		AY8910_Write(nDevice, nValue, 0);
#else
	if (g_bPhasorEnable)
	{
		int nAY_CS = (g_phasorMode == PH_Phasor) ? (~(value >> 3) & 3) : 1;

		if (nAY_CS & 1)
			AY8910_Write(device, value, 0);

		if (nAY_CS & 2)
			AY8910_Write(device, value, 1);
	}
	else
	{
		AY8910_Write(device, value, 0);
	}
#endif
}

//-----------------------------------------------------------------------------

void MockingboardCard::UpdateIFRandIRQ(SY6522_AY8910* pMB, BYTE clr_mask, BYTE set_mask)
{
	pMB->sy6522.UpdateIFR(clr_mask, set_mask);	// which calls MB_UpdateIRQ()
}

// Called from class SY6522
void MockingboardCard::MB_UpdateIRQ(void)
{
	// Now update the IRQ signal from all 6522s
	// . OR-sum of all active TIMER1, TIMER2 & SPEECH sources (from all 6522s)
	UINT bIRQ = 0;
	for (UINT i=0; i<NUM_SY6522; i++)
		bIRQ |= g_MB[i].sy6522.GetReg(SY6522::rIFR) & 0x80;

	// NB. Mockingboard generates IRQ on both 6522s:
	// . SSI263's IRQ (A/!R) is routed via the 2nd 6522 (at $Cn80) and must generate a 6502 IRQ (not NMI)
	//   - NB. 2nd SSI263's IRQ is routed via the 1st 6522 (at $Cn00) and again generates a 6502 IRQ
	// . SC-01's IRQ (A/!R) is routed via the 6522 at $Cn00 (NB. Only the Mockingboard "Sound/Speech I" card supports the SC-01)
	// Phasor's SSI263 IRQ (A/!R) line is *also* wired directly to the 6502's IRQ (as well as the 6522's CA1)

	if (bIRQ)
	    CpuIrqAssert(IS_6522);
	else
	    CpuIrqDeassert(IS_6522);
}

//===========================================================================

// Called by:
// . MB_SyncEventCallback() on a TIMER1 (not TIMER2) underflow - when IsAnyTimer1Active() == true
// . Update()                                                  - when IsAnyTimer1Active() == false
void MockingboardCard::MB_UpdateInternal(void)
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
		if (!g_nMB_InActiveCycleCount)
		{
			g_nMB_InActiveCycleCount = g_nCumulativeCycles;
		}
		else if (g_nCumulativeCycles - g_nMB_InActiveCycleCount > (unsigned __int64)g_fCurrentCLK6502/10)
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
	if (nNumSamples <= 0)
		nNumSamples = 0;
	if (nNumSamples > 2*nNumSamplesPerPeriod)
		nNumSamples = 2*nNumSamplesPerPeriod;

	if (nNumSamples > MAX_SAMPLES)
		nNumSamples = MAX_SAMPLES;	// Clamp to prevent buffer overflow

	if (nNumSamples)
		for (int nChip=0; nChip<NUM_AY8913; nChip++)
			AY8910Update(nChip, &ppAYVoiceBuffer[nChip*NUM_VOICES_PER_AY8913], nNumSamples);

	//

	DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
	HRESULT hr = MockingboardVoice.lpDSBvoice->GetCurrentPosition(&dwCurrentPlayCursor, &dwCurrentWriteCursor);
	if (FAILED(hr))
		return;

	static DWORD dwByteOffset = (DWORD)-1;
	if (dwByteOffset == (DWORD)-1)
	{
		// First time in this func

		dwByteOffset = dwCurrentWriteCursor;
	}
	else
	{
		// Check that our offset isn't between Play & Write positions

		if (dwCurrentWriteCursor > dwCurrentPlayCursor)
		{
			// |-----PxxxxxW-----|
			if ((dwByteOffset > dwCurrentPlayCursor) && (dwByteOffset < dwCurrentWriteCursor))
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
			if ((dwByteOffset > dwCurrentPlayCursor) || (dwByteOffset < dwCurrentWriteCursor))
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
	if (nBytesRemaining < 0)
		nBytesRemaining += g_dwDSBufferSize;

	// Calc correction factor so that play-buffer doesn't under/overflow
	const int nErrorInc = SoundCore_GetErrorInc();
	if (nBytesRemaining < g_dwDSBufferSize / 4)
		nNumSamplesError += nErrorInc;				// < 0.25 of buffer remaining
	else if (nBytesRemaining > g_dwDSBufferSize / 2)
		nNumSamplesError -= nErrorInc;				// > 0.50 of buffer remaining
	else
		nNumSamplesError = 0;						// Acceptable amount of data in buffer

#ifdef DBG_MB_UPDATE
	double fTicksSecs = (double)GetTickCount() / 1000.0;
	LogOutput("%010.3f: [MBUpdt]    PC=%08X, WC=%08X, Diff=%08X, Off=%08X, NS=%08X, NSE=%08X, Interval=%f\n", fTicksSecs, dwCurrentPlayCursor, dwCurrentWriteCursor, dwCurrentWriteCursor - dwCurrentPlayCursor, dwByteOffset, nNumSamples, nNumSamplesError, updateInterval);
#endif

	if (nNumSamples == 0)
		return;

	//

	const double fAttenuation = g_bPhasorEnable ? 2.0/3.0 : 1.0;

	for (int i=0; i<nNumSamples; i++)
	{
		// Mockingboard stereo (all voices on an AY8910 wire-or'ed together)
		// L = Address.b7=0, R = Address.b7=1
		int nDataL = 0, nDataR = 0;

		for (UINT j=0; j<NUM_VOICES_PER_AY8913; j++)
		{
			// Slot4
			nDataL += (int) ((double)ppAYVoiceBuffer[0*NUM_VOICES_PER_AY8913+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[1*NUM_VOICES_PER_AY8913+j][i] * fAttenuation);

			// Slot5
			nDataL += (int) ((double)ppAYVoiceBuffer[2*NUM_VOICES_PER_AY8913+j][i] * fAttenuation);
			nDataR += (int) ((double)ppAYVoiceBuffer[3*NUM_VOICES_PER_AY8913+j][i] * fAttenuation);
		}

		// Cap the superpositioned output
		if (nDataL < nWaveDataMin)
			nDataL = nWaveDataMin;
		else if (nDataL > nWaveDataMax)
			nDataL = nWaveDataMax;

		if (nDataR < nWaveDataMin)
			nDataR = nWaveDataMin;
		else if (nDataR > nWaveDataMax)
			nDataR = nWaveDataMax;

		g_nMixBuffer[i*g_nMB_NumChannels+0] = (short)nDataL;	// L
		g_nMixBuffer[i*g_nMB_NumChannels+1] = (short)nDataR;	// R
	}

	//

	DWORD dwDSLockedBufferSize0, dwDSLockedBufferSize1;
	SHORT *pDSLockedBuffer0, *pDSLockedBuffer1;

	hr = DSGetLock(MockingboardVoice.lpDSBvoice,
		dwByteOffset, (DWORD)nNumSamples * sizeof(short) * g_nMB_NumChannels,
		&pDSLockedBuffer0, &dwDSLockedBufferSize0,
		&pDSLockedBuffer1, &dwDSLockedBufferSize1);
	if (FAILED(hr))
		return;

	memcpy(pDSLockedBuffer0, &g_nMixBuffer[0], dwDSLockedBufferSize0);
	if (pDSLockedBuffer1)
		memcpy(pDSLockedBuffer1, &g_nMixBuffer[dwDSLockedBufferSize0/sizeof(short)], dwDSLockedBufferSize1);

	// Commit sound buffer
	hr = MockingboardVoice.lpDSBvoice->Unlock((void*)pDSLockedBuffer0, dwDSLockedBufferSize0,
											  (void*)pDSLockedBuffer1, dwDSLockedBufferSize1);

	dwByteOffset = (dwByteOffset + (DWORD)nNumSamples*sizeof(short)*g_nMB_NumChannels) % g_dwDSBufferSize;

#ifdef RIFF_MB
	RiffPutSamples(&g_nMixBuffer[0], nNumSamples);
#endif
}

void MockingboardCard::MB_Update(void)
{
#ifdef LOG_PERF_TIMINGS
	extern UINT64 g_timeMB_NoTimer;
	extern UINT64 g_timeMB_Timer;
	PerfMarker perfMarker(!IsAnyTimer1Active() == kTIMERDEVICE_INVALID ? g_timeMB_NoTimer : g_timeMB_Timer);
#endif

	MB_UpdateInternal();
}

//-----------------------------------------------------------------------------

bool MockingboardCard::MB_DSInit(void)
{
	LogFileOutput("MB_DSInit\n");
#ifdef NO_DIRECT_X

	return false;

#else // NO_DIRECT_X

	//
	// Create single Mockingboard voice
	//

	if (!g_bDSAvailable)
		return false;

	HRESULT hr = DSGetSoundBuffer(&MockingboardVoice, DSBCAPS_CTRLVOLUME, g_dwDSBufferSize, SAMPLE_RATE, g_nMB_NumChannels, "MB");
	LogFileOutput("MB_DSInit: DSGetSoundBuffer(), hr=0x%08X\n", hr);
	if (FAILED(hr))
	{
		LogFileOutput("MB_DSInit: DSGetSoundBuffer failed (%08X)\n", hr);
		return false;
	}

	bool bRes = DSZeroVoiceBuffer(&MockingboardVoice, g_dwDSBufferSize);
	LogFileOutput("MB_DSInit: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
	if (!bRes)
		return false;

	MockingboardVoice.bActive = true;

	// Volume might've been setup from value in Registry
	if (!MockingboardVoice.nVolume)
		MockingboardVoice.nVolume = DSBVOLUME_MAX;

	hr = MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
	LogFileOutput("MB_DSInit: SetVolume(), hr=0x%08X\n", hr);

	//---------------------------------

	for (UINT i=0; i<NUM_SSI263; i++)
	{
		if (!g_MB[i].ssi263.DSInit())
			return false;
	}

	return true;

#endif // NO_DIRECT_X
}

void MockingboardCard::MB_DSUninit(void)
{
	if (MockingboardVoice.lpDSBvoice && MockingboardVoice.bActive)
		DSVoiceStop(&MockingboardVoice);

	DSReleaseSoundBuffer(&MockingboardVoice);

	//

	for (UINT i=0; i< NUM_SSI263; i++)
		g_MB[i].ssi263.DSUninit();
}

void MockingboardCard::MB_Initialize(void)
{
	for (int id = 0; id < kNumSyncEvents; id++)
	{
		int syncId = (m_slot << 4) + id;	// NB. Encode the slot# into the id - used by MB_SyncEventCallback()
		g_syncEvent[id] = new SyncEvent(syncId, 0, MB_SyncEventCallback);
	}

	LogFileOutput("MB_Initialize: g_bDisableDirectSound=%d, g_bDisableDirectSoundMockingboard=%d\n", g_bDisableDirectSound, g_bDisableDirectSoundMockingboard);
	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
	{
		MockingboardVoice.bMute = true;
	}
	else
	{
		for (UINT i=0; i<NUM_VOICES; i++)
			ppAYVoiceBuffer[i] = new short [MAX_SAMPLES];	// Buffer can hold a max of 0.37 seconds worth of samples (16384/44100)

		AY8910_InitAll((int)g_fCurrentCLK6502, SAMPLE_RATE);
		LogFileOutput("MB_Initialize: AY8910_InitAll()\n");

		for (UINT i=0; i<NUM_SY6522; i++)
		{
			g_MB[i] = SY6522_AY8910();
			g_MB[i].nAY8910Number = i;
			const UINT id0 = i * SY6522::kNumTimersPer6522 + 0;	// TIMER1
			const UINT id1 = i * SY6522::kNumTimersPer6522 + 1;	// TIMER2
			g_MB[i].sy6522.InitSyncEvents(g_syncEvent[id0], g_syncEvent[id1]);
			g_MB[i].ssi263.SetDevice(i);
		}

		//

		g_bMBAvailable = MB_DSInit();
		LogFileOutput("MB_Initialize: MB_DSInit(), g_bMBAvailable=%d\n", g_bMBAvailable);

		Reset(true);
		LogFileOutput("MB_Initialize: Reset()\n");
	}
}

// NB. Mockingboard voice is *already* muted because showing 'Select Load State file' dialog
// . and voice will be unmuted when dialog is closed
void MockingboardCard::InitializeForLoadingSnapshot(void)	// GH#609
{
	Reset(true);

	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

	_ASSERT(MockingboardVoice.lpDSBvoice);
	DSVoiceStop(&MockingboardVoice);			// Reason: 'MB voice is playing' then loading a save-state where 'no MB present'

	// NB. ssi263.Stop() already done by Reset()
}

//-----------------------------------------------------------------------------

// NB. Called when /g_fCurrentCLK6502/ changes
void MockingboardCard::ReinitializeClock(void)
{
	AY8910_InitClock((int)g_fCurrentCLK6502);	// todo: account for g_PhasorClockScaleFactor?
												// NB. Other calls to AY8910_InitClock() use the constant CLK_6502
}

//-----------------------------------------------------------------------------

void MockingboardCard::Destroy(void)
{
	MB_DSUninit();

	for (int i=0; i<NUM_VOICES; i++)
		delete [] ppAYVoiceBuffer[i];

	for (int id=0; id<kNumSyncEvents; id++)
	{
		if (g_syncEvent[id] && g_syncEvent[id]->m_active)
			g_SynchronousEventMgr.Remove(id);

		delete g_syncEvent[id];
		g_syncEvent[id] = NULL;
	}
}

//-----------------------------------------------------------------------------

void MockingboardCard::Reset(const bool powerCycle)	// CTRL+RESET or power-cycle
{
	if (!g_bDSAvailable)
		return;

	for (int i=0; i<NUM_AY8913; i++)
	{
		g_MB[i].sy6522.Reset(powerCycle);

		AY8910_reset(i);
		g_MB[i].nAYCurrentRegister = 0;
		g_MB[i].state = AY_INACTIVE;
		g_MB[i].stateB = AY_INACTIVE;

		g_MB[i].ssi263.SetCardMode(g_phasorMode);
		g_MB[i].ssi263.Reset();
	}

	// Reset state
	{
		SetCumulativeCycles();

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
			{
				int syncId = (m_slot << 4) + id;
				g_SynchronousEventMgr.Remove(syncId);
			}
		}

		// Not these, as they don't change on a CTRL+RESET or power-cycle:
//		g_bMBAvailable = false;
//		g_bPhasorEnable = false;
	}

	ReinitializeClock();	// Reset CLK for AY8910s
}

//-----------------------------------------------------------------------------

// Echo+ mode - Phasor's 2nd 6522 is mapped to every 16-byte offset in $Cnxx (Echo+ has a single 6522 controlling two AY-3-8913's)

BYTE __stdcall MockingboardCard::IORead(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	UINT slot = (nAddr >> 8) & 0xf;
	MockingboardCard* pCard = (MockingboardCard*)MemGetSlotParameters(slot);
	return pCard->IOReadInternal(PC, nAddr, bWrite, nValue, nExecutedCycles);
}

BYTE MockingboardCard::IOReadInternal(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	UpdateCycles(nExecutedCycles);

#ifdef _DEBUG
	if (!IS_APPLE2 && MemCheckINTCXROM())
	{
		_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
		return mem[nAddr];
	}
#endif

	BYTE nMB = (nAddr>>8)&0xf - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if (g_bPhasorEnable)
	{
		if (nMB != 0)	// Slot4 only
			return MemReadFloatingBus(nExecutedCycles);

		int CS = 0;
		if (g_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (g_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (g_phasorMode == PH_EchoPlus)
			CS = 2;

		BYTE nRes = 0;

		if (CS & 1)
			nRes |= g_MB[nMB * NUM_DEVS_PER_MB + SY6522_DEVICE_A].sy6522.Read(nAddr & 0xf);

		if (CS & 2)
			nRes |= g_MB[nMB * NUM_DEVS_PER_MB + SY6522_DEVICE_B].sy6522.Read(nAddr & 0xf);

		bool bAccessedDevice = (CS & 3) ? true : false;

		bool CS_SSI263 = !(nAddr & 0x80) && (nAddr & 0x60);			// SSI263 at $Cn2x and/or $Cn4x

		if (g_phasorMode == PH_Phasor && CS_SSI263)					// NB. Mockingboard mode: SSI263.bit7 not readable
		{
			_ASSERT(!bAccessedDevice);
			if (nAddr & 0x40)	// Primary SSI263
				nRes = g_MB[nMB * NUM_DEVS_PER_MB + 1].ssi263.Read(nExecutedCycles);		// SSI263 only drives bit7
			if (nAddr & 0x20)	// Secondary SSI263
				nRes = g_MB[nMB * NUM_DEVS_PER_MB + 0].ssi263.Read(nExecutedCycles);		// SSI263 only drives bit7
			bAccessedDevice = true;
		}

		return bAccessedDevice ? nRes : MemReadFloatingBus(nExecutedCycles);
	}

#if DBG_MB_SS_CARD
	if (nMB == 1)
		return MemReadFloatingBus(nExecutedCycles);
#endif

	// NB. Mockingboard: SSI263.bit7 not readable (TODO: check this with real h/w)
	const BYTE device = nMB * NUM_DEVS_PER_MB + ((nOffset < SY6522B_Offset) ? SY6522_DEVICE_A : SY6522_DEVICE_B);
	const BYTE reg = nAddr & 0xf;
	return g_MB[device].sy6522.Read(reg);
}

//-----------------------------------------------------------------------------

BYTE __stdcall MockingboardCard::IOWrite(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	UINT slot = (nAddr >> 8) & 0xf;
	MockingboardCard* pCard = (MockingboardCard*)MemGetSlotParameters(slot);
	return pCard->IOWriteInternal(PC, nAddr, bWrite, nValue, nExecutedCycles);
}

BYTE MockingboardCard::IOWriteInternal(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	UpdateCycles(nExecutedCycles);

#ifdef _DEBUG
	if (!IS_APPLE2 && MemCheckINTCXROM())
	{
		_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
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
					IOReadInternal(PC, nAddr, 0, 0, nExecutedCycles);
			}
		}
	}

	BYTE nMB = ((nAddr>>8)&0xf) - SLOT4;
	BYTE nOffset = nAddr&0xff;

	if (g_bPhasorEnable)
	{
		if (nMB != 0)	// Slot4 only
			return 0;

		int CS = 0;
		if (g_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (g_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (g_phasorMode == PH_EchoPlus)
			CS = 2;

		if (CS & 1)
		{
			const BYTE device = nMB * NUM_DEVS_PER_MB + SY6522_DEVICE_A;
			const BYTE reg = nAddr & 0xf;
			g_MB[device].sy6522.Write(reg, nValue);
			if (reg == SY6522::rORB)
				WriteToORB(device);
		}

		if (CS & 2)
		{
			const BYTE device = nMB * NUM_DEVS_PER_MB + SY6522_DEVICE_B;
			const BYTE reg = nAddr & 0xf;
			g_MB[device].sy6522.Write(reg, nValue);
			if (reg == SY6522::rORB)
				WriteToORB(device);
		}

		bool CS_SSI263 = !(nAddr & 0x80) && (nAddr & 0x60);				// SSI263 at $Cn2x and/or $Cn4x

		if ((g_phasorMode == PH_Mockingboard || g_phasorMode == PH_Phasor) && CS_SSI263)	// No SSI263 for Echo+
		{
			// NB. Mockingboard mode: writes to $Cn4x/SSI263 also get written to 1st 6522 (have confirmed on real Phasor h/w)
			_ASSERT( (g_phasorMode == PH_Mockingboard && (CS==0 || CS==1)) || (g_phasorMode == PH_Phasor && (CS==0)) );
			if (nAddr & 0x40)	// Primary SSI263
				g_MB[nMB * NUM_DEVS_PER_MB + 1].ssi263.Write(nAddr&0x7, nValue);	// 2nd 6522 is used for 1st speech chip
			if (nAddr & 0x20)	// Secondary SSI263
				g_MB[nMB * NUM_DEVS_PER_MB + 0].ssi263.Write(nAddr&0x7, nValue);	// 1st 6522 is used for 2nd speech chip
		}

		return 0;
	}

	const BYTE device = nMB * NUM_DEVS_PER_MB + ((nOffset < SY6522B_Offset) ? SY6522_DEVICE_A : SY6522_DEVICE_B);
	const BYTE reg = nAddr & 0xf;
	g_MB[device].sy6522.Write(reg, nValue);
	if (reg == SY6522::rORB)
		WriteToORB(device);

#if !DBG_MB_SS_CARD
	if (nAddr & 0x40)
		g_MB[nMB * NUM_DEVS_PER_MB + 1].ssi263.Write(nAddr&0x7, nValue);		// 2nd 6522 is used for 1st speech chip
	if (nAddr & 0x20)
		g_MB[nMB * NUM_DEVS_PER_MB + 0].ssi263.Write(nAddr&0x7, nValue);		// 1st 6522 is used for 2nd speech chip
#endif

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
// . $C0C5 -> remaining in Echo+ mode (b#111)
// So $C0C5 seemingly results in 2 different modes.
//

BYTE __stdcall MockingboardCard::PhasorIO(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	UINT slot = ((nAddr & 0xff) >> 4) - 8;
	MockingboardCard* pCard = (MockingboardCard*)MemGetSlotParameters(slot);
	return pCard->PhasorIOInternal(PC, nAddr, bWrite, nValue, nExecutedCycles);
}

BYTE MockingboardCard::PhasorIOInternal(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nExecutedCycles)
{
	if (g_bPhasorEnable)
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

	for (UINT i=0; i<NUM_AY8913; i++)
		g_MB[i].ssi263.SetCardMode(g_phasorMode);

	return MemReadFloatingBus(nExecutedCycles);
}

//-----------------------------------------------------------------------------

void MockingboardCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	if (QueryType() == CT_MockingboardC)
		RegisterIoHandler(m_slot, IO_Null, IO_Null, IORead, IOWrite, NULL, NULL);
	else	// Phasor
		RegisterIoHandler(m_slot, PhasorIO, PhasorIO, IORead, IOWrite, NULL, NULL);

	g_bPhasorEnable = (QueryType() == CT_Phasor);

	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

	// Sound buffer may have been stopped by MB_InitializeForLoadingSnapshot().
	// NB. DSZeroVoiceBuffer() also zeros the sound buffer, so it's better than directly calling IDirectSoundBuffer::Play():
	// - without zeroing, then the previous sound buffer can be heard for a fraction of a second
	// - eg. when doing Mockingboard playback, then loading a save-state which is also doing Mockingboard playback
	DSZeroVoiceBuffer(&MockingboardVoice, g_dwDSBufferSize);
}

//-----------------------------------------------------------------------------

void MockingboardCard::MuteControl(bool mute)
{
	if (mute)
	{
		if (MockingboardVoice.bActive && !MockingboardVoice.bMute)
		{
			MockingboardVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
			MockingboardVoice.bMute = true;
		}

		for (UINT i = 0; i < NUM_AY8913; i++)
			g_MB[i].ssi263.Mute();
	}
	else
	{
		if (MockingboardVoice.bActive && MockingboardVoice.bMute)
		{
			MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);
			MockingboardVoice.bMute = false;
		}

		for (UINT i = 0; i < NUM_AY8913; i++)
			g_MB[i].ssi263.Unmute();
	}
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG
void MockingboardCard::CheckCumulativeCycles(void)
{
	_ASSERT(g_uLastCumulativeCycles == g_nCumulativeCycles);
	g_uLastCumulativeCycles = g_nCumulativeCycles;
}
#endif

// Called by: ResetState() and Snapshot_LoadState_v2()
void MockingboardCard::SetCumulativeCycles(void)
{
	g_uLastCumulativeCycles = g_nCumulativeCycles;
}

// Called by ContinueExecution() at the end of every execution period (~1000 cycles or ~3 cycles when MODE_STEPPING)
// NB. Required for FT's TEST LAB #1 player
void MockingboardCard::Update(const ULONG executedCycles)
{
	for (UINT i=0; i<NUM_AY8913; i++)
		g_MB[i].ssi263.PeriodicUpdate(executedCycles);

	if (IsAnyTimer1Active())
		return;

	// No 6522 TIMER1's are active, so periodically update AY8913's here...

	const UINT kCyclesPerAudioFrame = 1000;
	g_cyclesThisAudioFrame += executedCycles;
	if (g_cyclesThisAudioFrame < kCyclesPerAudioFrame)
		return;

	g_cyclesThisAudioFrame %= kCyclesPerAudioFrame;

	MB_Update();
}

//-----------------------------------------------------------------------------

// Called by:
// . CpuExecute() every ~1000 cycles @ 1MHz (or ~3 cycles when MODE_STEPPING)
// . MB_SyncEventCallback() on a TIMER1/2 underflow
// . MB_Read() / MB_Write() (for both normal & full-speed)
void MockingboardCard::UpdateCycles(ULONG executedCycles)
{
	CpuCalcCycles(executedCycles);
	UINT64 uCycles = g_nCumulativeCycles - g_uLastCumulativeCycles;
	_ASSERT(uCycles >= 0);
	if (uCycles == 0)
		return;

	g_uLastCumulativeCycles = g_nCumulativeCycles;
	_ASSERT(uCycles < 0x10000 || g_nAppMode == MODE_BENCHMARK);
	USHORT nClocks = (USHORT)uCycles;

	for (int i = 0; i < NUM_SY6522; i++)
	{
		g_MB[i].sy6522.UpdateTimer1(nClocks);
		g_MB[i].sy6522.UpdateTimer2(nClocks);
	}
}

//-----------------------------------------------------------------------------

// Called on a 6522 TIMER1/2 underflow
int MockingboardCard::MB_SyncEventCallback(int id, int /*cycles*/, ULONG uExecutedCycles)
{
	UINT slot = (id >> 4);
	MockingboardCard* pCard = (MockingboardCard*)MemGetSlotParameters(slot);
	return pCard->MB_SyncEventCallbackInternal(id, 0, uExecutedCycles);
}

int MockingboardCard::MB_SyncEventCallbackInternal(int id, int /*cycles*/, ULONG uExecutedCycles)
{
	UpdateCycles(uExecutedCycles);	// Underflow: so keep TIMER1/2 counters in sync

	SY6522_AY8910* pMB = &g_MB[(id & 0xf) / SY6522::kNumTimersPer6522];

	if ((id & 1) == 0)
	{
		_ASSERT(pMB->sy6522.IsTimer1Active());
		UpdateIFRandIRQ(pMB, 0, SY6522::IxR_TIMER1);
		MB_Update();

		if ((pMB->sy6522.GetReg(SY6522::rACR) & SY6522::ACR_RUNMODE) == SY6522::ACR_RM_FREERUNNING)
		{
			pMB->sy6522.StartTimer1();
			return pMB->sy6522.GetRegT1C() + SY6522::kExtraTimerCycles;
		}

		// One-shot mode
		// - Phasor's playback code uses one-shot mode

		pMB->sy6522.StopTimer1();
		return 0;			// Don't repeat event
	}
	else
	{
		// NB. Since not calling MB_Update(), then AppleWin doesn't (accurately?) support AY-playback using T2 (which is one-shot only)
		_ASSERT(pMB->sy6522.IsTimer2Active());
		UpdateIFRandIRQ(pMB, 0, SY6522::IxR_TIMER2);

		pMB->sy6522.StopTimer2();	// TIMER2 only runs in one-shot mode
		return 0;			// Don't repeat event
	}
}

//-----------------------------------------------------------------------------

bool MockingboardCard::IsActive(void)
{
	if (!MockingboardVoice.bActive)
		return false;

	bool isSSI263Active = false;
	for (UINT i=0; i<NUM_SSI263; i++)
		isSSI263Active |= g_MB[i].ssi263.IsPhonemeActive();

	return g_bMB_Active || isSSI263Active;
}

//-----------------------------------------------------------------------------

#if 0
DWORD MockingboardCard::MB_GetVolume(void)
{
	return MockingboardVoice.dwUserVolume;
}
#endif

void MockingboardCard::SetVolume(DWORD volume, DWORD volumeMax)
{
	MockingboardVoice.dwUserVolume = volume;
	MockingboardVoice.nVolume = NewVolume(volume, volumeMax);

	if (MockingboardVoice.bActive && !MockingboardVoice.bMute)
		MockingboardVoice.lpDSBvoice->SetVolume(MockingboardVoice.nVolume);

	for (UINT i=0; i<NUM_SSI263; i++)
		g_MB[i].ssi263.SetVolume(volume, volumeMax);
}

//---------------------------------------------------------------------------

// Called from class SSI263
UINT64 MockingboardCard::MB_GetLastCumulativeCycles(void)
{
	return g_uLastCumulativeCycles;
}

void MockingboardCard::MB_UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask)
{
	UpdateIFRandIRQ(&g_MB[nDevice], clr_mask, set_mask);
}

BYTE MockingboardCard::MB_GetPCR(BYTE nDevice)
{
	return g_MB[nDevice].sy6522.GetReg(SY6522::rPCR);
}

//===========================================================================

#include "SaveState_Structs_v1.h"

// Called by debugger - Debugger_Display.cpp
void MockingboardCard::GetSnapshot_v1(SS_CARD_MOCKINGBOARD_v1* const pSS)
{
	pSS->Hdr.UnitHdr.hdr.v2.Length = sizeof(SS_CARD_MOCKINGBOARD_v1);
	pSS->Hdr.UnitHdr.hdr.v2.Type = UT_Card;
	pSS->Hdr.UnitHdr.hdr.v2.Version = 1;

	pSS->Hdr.Slot = m_slot;
	pSS->Hdr.Type = CT_MockingboardC;

	SY6522_AY8910* pMB = &g_MB[0];

	for (UINT i=0; i<MB_UNITS_PER_CARD_v1; i++)
	{
		// 6522
		pMB->sy6522.GetRegs((BYTE*)&pSS->Unit[i].RegsSY6522);	// continuous 16-byte array

		// AY8913
		for (UINT j=0; j<16; j++)
		{
			pSS->Unit[i].RegsAY8910[j] = AYReadReg(i, j);
		}

		memset(&pSS->Unit[i].RegsSSI263, 0, sizeof(SSI263A));	// Not used by debugger
		pSS->Unit[i].nAYCurrentRegister = pMB->nAYCurrentRegister;
		pSS->Unit[i].bTimer1Active = pMB->sy6522.IsTimer1Active();
		pSS->Unit[i].bTimer2Active = pMB->sy6522.IsTimer2Active();
		pSS->Unit[i].bSpeechIrqPending = false;

		pMB++;
	}
}

//=============================================================================

// Unit version history:
// 2: Added: Timer1 & Timer2 active
// 3: Added: Unit state - GH#320
// 4: Added: 6522 timerIrqDelay - GH#652
// 5: Added: Unit state-B (Phasor only) - GH#659
// 6: Changed SS_YAML_KEY_PHASOR_MODE from (0,1) to (0,5,7)
//    Added SS_YAML_KEY_VOTRAX_PHONEME
//    Removed: redundant SS_YAML_KEY_PHASOR_CLOCK_SCALE_FACTOR
// 7: Added SS_YAML_KEY_SSI263_REG_ACTIVE_PHONEME to SSI263 sub-unit
// 8: Moved Timer1 & Timer2 active to 6522 sub-unit
//    Removed Timer1/Timer2/Speech IRQ Pending
const UINT kUNIT_VERSION = 8;

const UINT NUM_MB_UNITS = 2;
const UINT NUM_PHASOR_UNITS = 2;

#define SS_YAML_KEY_MB_UNIT "Unit"
#define SS_YAML_KEY_AY_CURR_REG "AY Current Register"
#define SS_YAML_KEY_MB_UNIT_STATE "Unit State"
#define SS_YAML_KEY_MB_UNIT_STATE_B "Unit State-B"	// Phasor only
#define SS_YAML_KEY_TIMER1_IRQ "Timer1 IRQ Pending"	// v8: deprecated
#define SS_YAML_KEY_TIMER2_IRQ "Timer2 IRQ Pending"	// v8: deprecated
#define SS_YAML_KEY_SPEECH_IRQ "Speech IRQ Pending"	// v8: deprecated
#define SS_YAML_KEY_TIMER1_ACTIVE "Timer1 Active"	// v8: move to 6522 sub-unit
#define SS_YAML_KEY_TIMER2_ACTIVE "Timer2 Active"	// v8: move to 6522 sub-unit

#define SS_YAML_KEY_PHASOR_UNIT "Unit"
#define SS_YAML_KEY_PHASOR_CLOCK_SCALE_FACTOR "Clock Scale Factor"	// v6: deprecated
#define SS_YAML_KEY_PHASOR_MODE "Mode"

#define SS_YAML_KEY_VOTRAX_PHONEME "Votrax Phoneme"

std::string MockingboardCard::GetSnapshotCardName(void)
{
	static const std::string name("Mockingboard C");
	return name;
}

std::string MockingboardCard::GetSnapshotCardNamePhasor(void)
{
	static const std::string name("Phasor");
	return name;
}

void MockingboardCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (QueryType() == CT_Phasor)
		return Phasor_SaveSnapshot(yamlSaveHelper);

	//

	const UINT nMbCardNum = m_slot - SLOT4;
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, pMB->ssi263.GetVotraxPhoneme());

	for (UINT i=0; i<NUM_MB_UNITS; i++)
	{
		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_MB_UNIT, i);

		pMB->sy6522.SaveSnapshot(yamlSaveHelper);
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum, std::string(""));
		pMB->ssi263.SaveSnapshot(yamlSaveHelper);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);

		nDeviceNum++;
		pMB++;
	}
}

bool MockingboardCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (QueryType() == CT_Phasor)
		return Phasor_LoadSnapshot(yamlLoadHelper, version);

	//

	if (m_slot != 4 && m_slot != 5)	// fixme
		throw std::runtime_error("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::runtime_error("Card: wrong version");

	AY8910UpdateSetCycles();

	const UINT nMbCardNum = m_slot - SLOT4;	// FIXME
	UINT nDeviceNum = nMbCardNum*2;
	SY6522_AY8910* pMB = &g_MB[nDeviceNum];

	bool isVotrax = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;
	pMB->ssi263.SetVotraxPhoneme(isVotrax);

	for (UINT i=0; i<NUM_MB_UNITS; i++)
	{
		char szNum[2] = {char('0' + i), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::runtime_error("Card: Expected key: " + unit);

		pMB->sy6522.LoadSnapshot(yamlLoadHelper, version);
		UpdateIFRandIRQ(pMB, 0, pMB->sy6522.GetReg(SY6522::rIFR));			// Assert any pending IRQs (GH#677)
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum, std::string(""));
		pMB->ssi263.LoadSnapshot(yamlLoadHelper, nDeviceNum, PH_Mockingboard, version);		// Pre: SetVotraxPhoneme()

		pMB->nAYCurrentRegister = yamlLoadHelper.LoadUint(SS_YAML_KEY_AY_CURR_REG);

		if (version == 1)
		{
			pMB->sy6522.SetTimersActiveFromSnapshot(false, false, version);
		}
		else if (version >= 2 && version <= 7)
		{
			bool timer1Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_ACTIVE);
			bool timer2Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_ACTIVE);
			pMB->sy6522.SetTimersActiveFromSnapshot(timer1Active, timer2Active, version);
		}

		if (version <= 7)
		{
			yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_IRQ);	// Consume redundant data
			yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_IRQ);	// Consume redundant data
			yamlLoadHelper.LoadBool(SS_YAML_KEY_SPEECH_IRQ);	// Consume redundant data
		}

		pMB->state = AY_INACTIVE;
		pMB->stateB = AY_INACTIVE;
		if (version >= 3)
			pMB->state = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE) & 7);

		yamlLoadHelper.PopMap();

		//

		nDeviceNum++;
		pMB++;
	}

	AY8910_InitClock((int)Get6502BaseClock());

	// NB. g_bPhasorEnable setup in InitializeIO()

	return true;
}

void MockingboardCard::Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (m_slot != 4)
		throw std::runtime_error("Card: Phasor only supported in slot-4");

	UINT nDeviceNum = 0;
	SY6522_AY8910* pMB = &g_MB[0];	// fixme: Phasor uses MB's slot4(2x6522), slot4(2xSSI263), but slot4+5(4xAY8910)

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardNamePhasor(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveUint(SS_YAML_KEY_PHASOR_MODE, g_phasorMode);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, pMB->ssi263.GetVotraxPhoneme());

	for (UINT i=0; i<NUM_PHASOR_UNITS; i++)
	{
		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_PHASOR_UNIT, i);

		pMB->sy6522.SaveSnapshot(yamlSaveHelper);
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum+0, std::string("-A"));
		AY8910_SaveSnapshot(yamlSaveHelper, nDeviceNum+1, std::string("-B"));
		pMB->ssi263.SaveSnapshot(yamlSaveHelper);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE_B, pMB->stateB);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);

		nDeviceNum += 2;
		pMB++;
	}
}

bool MockingboardCard::Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (m_slot != 4)	// fixme
		throw std::runtime_error("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::runtime_error("Card: wrong version");

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

	AY8910UpdateSetCycles();

	UINT nDeviceNum = 0;
	SY6522_AY8910* pMB = &g_MB[0];

	bool isVotrax = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;
	pMB->ssi263.SetVotraxPhoneme(isVotrax);

	for (UINT i=0; i<NUM_PHASOR_UNITS; i++)
	{
		char szNum[2] = {char('0' + i), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::runtime_error("Card: Expected key: " + unit);

		pMB->sy6522.LoadSnapshot(yamlLoadHelper, version);
		UpdateIFRandIRQ(pMB, 0, pMB->sy6522.GetReg(SY6522::rIFR));			// Assert any pending IRQs (GH#677)
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum+0, std::string("-A"));
		AY8910_LoadSnapshot(yamlLoadHelper, nDeviceNum+1, std::string("-B"));
		pMB->ssi263.LoadSnapshot(yamlLoadHelper, nDeviceNum, PH_Phasor, version);	// Pre: SetVotraxPhoneme()

		pMB->nAYCurrentRegister = yamlLoadHelper.LoadUint(SS_YAML_KEY_AY_CURR_REG);

		if (version == 1)
		{
			pMB->sy6522.SetTimersActiveFromSnapshot(false, false, version);
		}
		else if (version >= 2 && version <= 7)
		{
			bool timer1Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_ACTIVE);
			bool timer2Active = yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_ACTIVE);
			pMB->sy6522.SetTimersActiveFromSnapshot(timer1Active, timer2Active, version);
		}

		if (version <= 7)
		{
			yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER1_IRQ);	// Consume redundant data
			yamlLoadHelper.LoadBool(SS_YAML_KEY_TIMER2_IRQ);	// Consume redundant data
			yamlLoadHelper.LoadBool(SS_YAML_KEY_SPEECH_IRQ);	// Consume redundant data
		}

		pMB->state = AY_INACTIVE;
		pMB->stateB = AY_INACTIVE;
		if (version >= 3)
			pMB->state = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE) & 7);
		if (version >= 5)
			pMB->stateB = (MockingboardUnitState_e) (yamlLoadHelper.LoadUint(SS_YAML_KEY_MB_UNIT_STATE_B) & 7);

		yamlLoadHelper.PopMap();

		//

		nDeviceNum += 2;
		pMB++;
	}

	AY8910_InitClock((int)(Get6502BaseClock() * g_PhasorClockScaleFactor));

	// NB. g_bPhasorEnable setup in InitializeIO()

	return true;
}
