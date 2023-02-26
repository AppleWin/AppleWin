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
#include "MockingboardDefs.h"
#include "6522.h"

#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Log.h"
#include "Memory.h"
#include "SoundCore.h"
#include "SynchronousEventManager.h"
#include "YamlHelper.h"

#include "AY8910.h"
#include "SSI263.h"

#define DBG_MB_SS_CARD 0	// NB. From UI, select Mockingboard (not Phasor)
#define DBG_SUPPORT_ECHOPLUS 0	// Allow Phasor (in Echo+ mode) to pass the TMS5220 detection used by Echo+ disk

//---------------------------------------------------------------------------

MockingboardCard::MockingboardCard(UINT slot, SS_CARDTYPE type) : Card(type, slot), m_MBSubUnit{slot, slot}
{
	m_lastCumulativeCycle = 0;
	m_lastAYUpdateCycle = 0;

	for (UINT i = 0; i < NUM_VOICES; i++)
		m_ppAYVoiceBuffer[i] = new short[MAX_SAMPLES];	// Buffer can hold a max of 0.37 seconds worth of samples (16384/44100)

	m_inActiveCycleCount = 0;
	m_regAccessedFlag = false;
	m_isActive = false;

	m_phasorEnable = (QueryType() == CT_Phasor);
	m_phasorMode = PH_Mockingboard;
	m_phasorClockScaleFactor = 1;

	m_lastMBUpdateCycle = 0;
	m_numSamplesError = 0;

	//

	for (int id = 0; id < kNumSyncEvents; id++)
	{
		int syncId = (m_slot << 4) + id;	// NB. Encode the slot# into the id - used by MB_SyncEventCallback()
		m_syncEvent[id] = new SyncEvent(syncId, 0, MB_SyncEventCallback);
	}

	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
	{
		m_MBSubUnit[i].nAY8910Number = i;
		const UINT id0 = i * SY6522::kNumTimersPer6522 + 0;	// TIMER1
		const UINT id1 = i * SY6522::kNumTimersPer6522 + 1;	// TIMER2
		m_MBSubUnit[i].sy6522.InitSyncEvents(m_syncEvent[id0], m_syncEvent[id1]);
		m_MBSubUnit[i].ssi263.SetDevice(i);
	}

	AY8910_InitAll((int)g_fCurrentCLK6502, SAMPLE_RATE);
	LogFileOutput("MockingboardCard::ctor: AY8910_InitAll()\n");

	Reset(true);
	LogFileOutput("MockingboardCard::ctor: Reset()\n");
}

MockingboardCard::~MockingboardCard(void)
{
	Destroy();
}

//---------------------------------------------------------------------------

bool MockingboardCard::IsAnyTimer1Active(void)
{
	bool active = false;
	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
		active |= m_MBSubUnit[i].sy6522.IsTimer1Active();
	return active;
}

//---------------------------------------------------------------------------

#ifdef _DEBUG
void MockingboardCard::Get6522IrqDescription(std::string& desc)
{
	bool isIRQ = false;
	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
	{
		if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IFR_IRQ)
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

	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
	{
		if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IFR_IRQ)
		{
			if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_TIMER1)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "TIMER1 ";
			}
			if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_TIMER2)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "TIMER2 ";
			}
			if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_VOTRAX)
			{
				desc += ((i&1)==0) ? "A:" : "B:";
				desc += "VOTRAX ";
			}
			if (m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & SY6522::IxR_SSI263)
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

void MockingboardCard::WriteToORB(BYTE subunit)
{
	BYTE value = m_MBSubUnit[subunit].sy6522.Read(SY6522::rORB);

	if (subunit == 0 && // SC01 only at $Cn00 (not $Cn80)
		m_MBSubUnit[subunit].sy6522.Read(SY6522::rPCR) == 0xB0)
	{
		// Votrax speech data
		const BYTE DDRB = m_MBSubUnit[subunit].sy6522.Read(SY6522::rDDRB);
		m_MBSubUnit[subunit].ssi263.Votrax_Write((value & DDRB) | (DDRB ^ 0xff));	// DDRB's zero bits (inputs) are high impedence, so output as 1 (GH#952)
		return;
	}

#if DBG_MB_SS_CARD
	if ((subunit & 1) == 1)
		AY8910_Write(subunit, 0, nValue);
#else
	if (m_phasorEnable)
	{
		const int kAY0 = 2;		// Phasor mode: bit4=0 (active low)  selects the 1st AY8913, ie. the only AY8913 in Mockingboard mode (confirmed on real Phasor h/w)
								// Echo+  mode: bit3=1 (active high) selects the 1st AY8913
		const int kAY1 = 1;		// Phasor mode: bit3=0 (active low)  selects the 2nd AY8913 attached to this 6522 (unavailable in Mockingboard mode)
								// Echo+  mode: bit4=1 (active high) selects the 2nd AY8913
		const int nAY_CS =	(m_phasorMode == PH_EchoPlus) ? ((value >> 4) & 1) | ((value >> 2) & 2)	// swap bits 4 & 3
							: (m_phasorMode == PH_Phasor) ? (~(value >> 3) & 3)
							: kAY0; // Anything else is Mockingboard

		if (m_phasorMode == PH_EchoPlus)
			subunit = SY6522_DEVICE_B;

		if (nAY_CS & kAY0)
			AY8910_Write(subunit, AY8913_DEVICE_A, value);

		if (nAY_CS & kAY1)
			AY8910_Write(subunit, AY8913_DEVICE_B, value);
	}
	else
	{
		AY8910_Write(subunit, AY8913_DEVICE_A, value);
	}
#endif
}

//-----------------------------------------------------------------------------

void MockingboardCard::AY8910_Write(BYTE subunit, BYTE ay, BYTE value)
{
	m_regAccessedFlag = true;
	MB_SUBUNIT* pMB = &m_MBSubUnit[subunit];
	SY6522& r6522 = (m_phasorEnable && m_phasorMode == PH_EchoPlus) ? m_MBSubUnit[SY6522_DEVICE_B].sy6522 : pMB->sy6522;

	if ((value & 4) == 0)
	{
		// RESET: Reset AY8910 only
		AY8910_reset(subunit, ay);
	}
	else
	{
		// Determine the AY8910 inputs
		int nBDIR = (value & 2) ? 1 : 0;
		const int nBC2 = 1;		// Hardwired to +5V
		int nBC1 = value & 1;

		MockingboardUnitState_e nAYFunc = (MockingboardUnitState_e) ((nBDIR<<2) | (nBC2<<1) | nBC1);
		MockingboardUnitState_e& state = (ay == AY8913_DEVICE_A) ? pMB->state : pMB->stateB;	// GH#659

#if _DEBUG
		if (!m_phasorEnable || m_phasorMode == PH_Mockingboard)
			_ASSERT(ay == AY8913_DEVICE_A);
		if (nAYFunc == AY_READ || nAYFunc == AY_WRITE || nAYFunc == AY_LATCH)
			_ASSERT(state == AY_INACTIVE);
#endif

		if (state == AY_INACTIVE)	// GH#320: functions only work from inactive state
		{
			switch (nAYFunc)
			{
				case AY_INACTIVE:	// 4: INACTIVE
					break;

				case AY_READ:		// 5: READ FROM PSG (need to set DDRA to input)
					r6522.SetRegORA( AYReadReg(subunit, ay, pMB->nAYCurrentRegister) & (r6522.GetReg(SY6522::rDDRA) ^ 0xff) );
					break;

				case AY_WRITE:		// 6: WRITE TO PSG
					_AYWriteReg(subunit, ay, pMB->nAYCurrentRegister, r6522.GetReg(SY6522::rORA));
					break;

				case AY_LATCH:		// 7: LATCH ADDRESS
					// http://www.worldofspectrum.org/forums/showthread.php?t=23327
					// Selecting an unused register number above 0x0f puts the AY into a state where
					// any values written to the data/address bus are ignored, but can be read back
					// within a few tens of thousands of cycles before they decay to zero.
					if (r6522.GetReg(SY6522::rORA) <= 0x0F)
						pMB->nAYCurrentRegister = r6522.GetReg(SY6522::rORA) & 0x0F;
					// else Pro-Mockingboard (clone from HK)
					break;
			}
		}

		state = nAYFunc;
	}
}

//-----------------------------------------------------------------------------

void MockingboardCard::UpdateIFRandIRQ(MB_SUBUNIT* pMB, BYTE clr_mask, BYTE set_mask)
{
	pMB->sy6522.UpdateIFR(clr_mask, set_mask);	// which calls UpdateIRQ()
}

//---------------------------------------------------------------------------

// Called from MockingboardCardMgr
bool MockingboardCard::Is6522IRQ(void)
{
	// Now update the IRQ signal from all 6522s
	// . OR-sum of all active TIMER1, TIMER2 & SPEECH sources (from all 6522s)
	bool irq = false;
	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
		irq |= m_MBSubUnit[i].sy6522.GetReg(SY6522::rIFR) & 0x80 ? true : false;

	// NB. Mockingboard generates IRQ on both 6522s:
	// . SSI263's IRQ (A/!R) is routed via the 2nd 6522 (at $Cn80) and must generate a 6502 IRQ (not NMI)
	//   - NB. 2nd SSI263's IRQ is routed via the 1st 6522 (at $Cn00) and again generates a 6502 IRQ
	// . SC-01's IRQ (A/!R) is routed via the 6522 at $Cn00 (NB. Only the Mockingboard "Sound/Speech I" card supports the SC-01)
	// Phasor's SSI263 IRQ (A/!R) line is *also* wired directly to the 6502's IRQ (as well as the 6522's CA1)

	return irq;
}

//---------------------------------------------------------------------------

// Called from class SSI263
UINT64 MockingboardCard::GetLastCumulativeCycles(void)
{
	return m_lastCumulativeCycle;
}

void MockingboardCard::UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask)
{
	UpdateIFRandIRQ(&m_MBSubUnit[nDevice], clr_mask, set_mask);
}

BYTE MockingboardCard::GetPCR(BYTE nDevice)
{
	return m_MBSubUnit[nDevice].sy6522.GetReg(SY6522::rPCR);
}

//===========================================================================

// Called by:
// . MB_SyncEventCallback() -> MockingboardCardManager::UpdateSoundBuffer() on a TIMER1 (not TIMER2) underflow - when IsAnyTimer1Active() == true (for any MB)
// . MockingboardCardManager::Update()                                                                         - when IsAnyTimer1Active() == false (for all MB's)
UINT MockingboardCard::MB_Update(void)
{
	if (g_bFullSpeed)
	{
		// Keep AY reg writes relative to the current 'frame'
		// - Required for Ultima3:
		//   . Tune ends
		//   . g_bFullSpeed:=true (disk-spinning) for ~50 frames
		//   . U3 sets AY_ENABLE:=0xFF (as a side-effect, this sets g_bFullSpeed:=false)
		//   o Without this, the write to AY_ENABLE gets ignored (since AY8910's /m_lastCumulativeCycle/ was last set 50 frame ago)
		AY8910UpdateSetCycles();

		// TODO:
		// If any AY regs have changed then push them out to the AY chip

		return 0;
	}

	//

	if (!m_regAccessedFlag)
	{
		if (!m_inActiveCycleCount)
		{
			m_inActiveCycleCount = g_nCumulativeCycles;
		}
		else if (g_nCumulativeCycles - m_inActiveCycleCount > (unsigned __int64)g_fCurrentCLK6502 / 10)
		{
			// After 0.1 sec of Apple time, assume MB is not active
			m_isActive = false;
		}
	}
	else
	{
		m_inActiveCycleCount = 0;
		m_regAccessedFlag = false;
		m_isActive = true;
	}

	//

	// For small timer periods, wait for a period of 500cy before updating DirectSound ring-buffer.
	// NB. A timer period of less than 24cy will yield nNumSamplesPerPeriod=0.
	const double kMinimumUpdateInterval = 500.0;	// Arbitary (500 cycles = 21 samples)
	const double kMaximumUpdateInterval = (double)(0xFFFF + 2);	// Max 6522 timer interval (2756 samples)

	if (m_lastMBUpdateCycle == 0)
		m_lastMBUpdateCycle = m_lastCumulativeCycle;		// Initial call to MB_Update() after reset/power-cycle

	_ASSERT(m_lastCumulativeCycle >= m_lastMBUpdateCycle);
	double updateInterval = (double)(m_lastCumulativeCycle - m_lastMBUpdateCycle);
	if (updateInterval < kMinimumUpdateInterval)
		return 0;
	if (updateInterval > kMaximumUpdateInterval)
		updateInterval = kMaximumUpdateInterval;

	m_lastMBUpdateCycle = m_lastCumulativeCycle;

	const double nIrqFreq = g_fCurrentCLK6502 / updateInterval + 0.5;			// Round-up
	const int nNumSamplesPerPeriod = (int)((double)SAMPLE_RATE / nIrqFreq);	// Eg. For 60Hz this is 735

	int nNumSamples = nNumSamplesPerPeriod + m_numSamplesError;					// Apply correction
	if (nNumSamples <= 0)
		nNumSamples = 0;
	if (nNumSamples > 2 * nNumSamplesPerPeriod)
		nNumSamples = 2 * nNumSamplesPerPeriod;

	if (nNumSamples > MAX_SAMPLES)
		nNumSamples = MAX_SAMPLES;	// Clamp to prevent buffer overflow

	if (nNumSamples)
	{
		for (BYTE subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
		{
			for (BYTE ay = 0; ay < NUM_AY8913_PER_SUBUNIT; ay++)
			{
				const UINT chip = subunit * NUM_AY8913_PER_SUBUNIT + ay;
				AY8910Update(subunit, ay, &m_ppAYVoiceBuffer[chip * NUM_VOICES_PER_AY8913], nNumSamples);
			}
		}

		// Echo+ right speaker is also output to left speaker
		if (m_phasorEnable && m_phasorMode == PH_EchoPlus)
		{
			for (UINT j = 0; j < NUM_VOICES_PER_AY8913; j++)
			{
				memcpy(m_ppAYVoiceBuffer[0 * NUM_VOICES_PER_AY8913 + j], m_ppAYVoiceBuffer[2 * NUM_VOICES_PER_AY8913 + j], nNumSamples * sizeof(short));
				memcpy(m_ppAYVoiceBuffer[1 * NUM_VOICES_PER_AY8913 + j], m_ppAYVoiceBuffer[3 * NUM_VOICES_PER_AY8913 + j], nNumSamples * sizeof(short));
			}
		}
	}

	return (UINT) nNumSamples;
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
	for (UINT i = 0; i < NUM_SSI263; i++)
		m_MBSubUnit[i].ssi263.DSUninit();

	for (UINT i = 0; i < NUM_VOICES; i++)
	{
		delete[] m_ppAYVoiceBuffer[i];
		m_ppAYVoiceBuffer[i] = NULL;
	}

	for (UINT id = 0; id < kNumSyncEvents; id++)
	{
		if (m_syncEvent[id] && m_syncEvent[id]->m_active)
			g_SynchronousEventMgr.Remove(m_syncEvent[id]->m_id);

		delete m_syncEvent[id];
		m_syncEvent[id] = NULL;
	}
}

//-----------------------------------------------------------------------------

void MockingboardCard::Reset(const bool powerCycle)	// CTRL+RESET or power-cycle
{
	for (BYTE subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		m_MBSubUnit[subunit].sy6522.Reset(powerCycle);

		for (BYTE ay = 0; ay < NUM_AY8913_PER_SUBUNIT; ay++)
			AY8910_reset(subunit, ay);

		m_MBSubUnit[subunit].nAYCurrentRegister = 0;
		m_MBSubUnit[subunit].state = AY_INACTIVE;
		m_MBSubUnit[subunit].stateB = AY_INACTIVE;

		m_MBSubUnit[subunit].ssi263.SetCardMode(PH_Mockingboard);	// Revert to PH_Mockingboard mode
		m_MBSubUnit[subunit].ssi263.Reset();
	}

	// Reset state
	{
		SetCumulativeCycles();

		m_inActiveCycleCount = 0;
		m_regAccessedFlag = false;
		m_isActive = false;

		m_phasorMode = PH_Mockingboard;
		m_phasorClockScaleFactor = 1;

		m_lastMBUpdateCycle = 0;

		for (int id = 0; id < kNumSyncEvents; id++)
		{
			if (m_syncEvent[id] && m_syncEvent[id]->m_active)
				g_SynchronousEventMgr.Remove(m_syncEvent[id]->m_id);
		}

		// Not this, since no change on a CTRL+RESET or power-cycle:
//		m_phasorEnable = false;
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
	GetCardMgr().GetMockingboardCardMgr().UpdateCycles(nExecutedCycles);

#ifdef _DEBUG
	if (!IS_APPLE2 && MemCheckINTCXROM())
	{
		_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
		return mem[nAddr];
	}
#endif

	if (m_phasorEnable)
	{
		int CS = 0;
		if (m_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (m_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (m_phasorMode == PH_EchoPlus)
			CS = 2;

		BYTE nRes = 0;

		if (CS & 1)
			nRes |= m_MBSubUnit[SY6522_DEVICE_A].sy6522.Read(nAddr & 0xf);

		if (CS & 2)
			nRes |= m_MBSubUnit[SY6522_DEVICE_B].sy6522.Read(nAddr & 0xf);

		bool bAccessedDevice = (CS & 3) ? true : false;

		bool CS_SSI263 = !(nAddr & 0x10) && (nAddr & 0x60) && !(nAddr & 0x80);				// SSI263 at $Cn2x and/or $Cn4x

		if (m_phasorMode == PH_Phasor && CS_SSI263)					// NB. Mockingboard mode: SSI263.bit7 not readable
		{
			_ASSERT(!bAccessedDevice);								// In Phasor native mode, 6522 & SSI263 are interleaved in $Cn10-$Cn7F card I/O memory
			if (nAddr & 0x40)	// Primary SSI263
				nRes = m_MBSubUnit[1].ssi263.Read(nExecutedCycles);		// SSI263 only drives bit7
			if (nAddr & 0x20)	// Secondary SSI263
				nRes = m_MBSubUnit[0].ssi263.Read(nExecutedCycles);		// SSI263 only drives bit7
			bAccessedDevice = true;
		}

		return bAccessedDevice ? nRes : MemReadFloatingBus(nExecutedCycles);
	}

#if DBG_MB_SS_CARD
	if (nMB == 1)
		return MemReadFloatingBus(nExecutedCycles);
#endif

	// NB. Mockingboard: SSI263.bit7 not readable (TODO: check this with real h/w)
	const BYTE offset = nAddr & 0xff;
	const BYTE subunit = (offset < SY6522B_Offset) ? SY6522_DEVICE_A : SY6522_DEVICE_B;
	const BYTE reg = nAddr & 0xf;
	return m_MBSubUnit[subunit].sy6522.Read(reg);
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
	GetCardMgr().GetMockingboardCardMgr().UpdateCycles(nExecutedCycles);

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

	if (m_phasorEnable)
	{
		int CS = 0;
		if (m_phasorMode == PH_Mockingboard)
			CS = ( ( nAddr & 0x80 ) >> 7 ) + 1;							// 1 or 2
		else if (m_phasorMode == PH_Phasor)
			CS = ( ( nAddr & 0x80 ) >> 6 ) | ( ( nAddr & 0x10 ) >> 4 );	// 0, 1, 2 or 3
		else if (m_phasorMode == PH_EchoPlus)
			CS = 2;

		if (CS & 1)
		{
			const BYTE reg = nAddr & 0xf;
			m_MBSubUnit[SY6522_DEVICE_A].sy6522.Write(reg, nValue);
			if (reg == SY6522::rORB)
				WriteToORB(SY6522_DEVICE_A);
		}

		if (CS & 2)
		{
			const BYTE reg = nAddr & 0xf;
			m_MBSubUnit[SY6522_DEVICE_B].sy6522.Write(reg, nValue);
			if (reg == SY6522::rORB)
				WriteToORB(SY6522_DEVICE_B);
		}

		bool CS_SSI263_A = (m_phasorMode == PH_Phasor)	? !(nAddr & 0x80) && (nAddr & 0x40)	// SSI263 at $Cn4x, $Cn6x
														: nAddr & 0x40;						// SSI263 at $Cn4x-Cn7x, $CnCx-CnFx

		bool CS_SSI263_B = (m_phasorMode == PH_Phasor)	? !(nAddr & 0x80) && (nAddr & 0x20)	// SSI263 at $Cn2x, $Cn6x
														: nAddr & 0x20;						// SSI263 at $Cn2x-Cn3x, $Cn6x-Cn7x, $CnAx-CnBx, $CnEx-CnFx

		if (m_phasorMode == PH_Mockingboard || m_phasorMode == PH_Phasor)	// No SSI263 for Echo+
		{
			// NB. Mockingboard mode: writes to $Cn4x/SSI263 also get written to 1st 6522 (have confirmed on real Phasor h/w)
			if (CS_SSI263_A)	// Primary SSI263
				m_MBSubUnit[1].ssi263.Write(nAddr&0x7, nValue);	// 2nd 6522 is used for 1st speech chip
			if (CS_SSI263_B)	// Secondary SSI263
				m_MBSubUnit[0].ssi263.Write(nAddr&0x7, nValue);	// 1st 6522 is used for 2nd speech chip
		}

		return 0;
	}

	const BYTE offset = nAddr & 0xff;
	const BYTE subunit = (offset < SY6522B_Offset) ? SY6522_DEVICE_A : SY6522_DEVICE_B;
	const BYTE reg = nAddr & 0xf;
	m_MBSubUnit[subunit].sy6522.Write(reg, nValue);
	if (reg == SY6522::rORB)
		WriteToORB(subunit);

#if !DBG_MB_SS_CARD
	if (nAddr & 0x40)
		m_MBSubUnit[1].ssi263.Write(nAddr&0x7, nValue);		// 2nd 6522 is used for 1st speech chip
	if (nAddr & 0x20)
		m_MBSubUnit[0].ssi263.Write(nAddr&0x7, nValue);		// 1st 6522 is used for 2nd speech chip
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
	if (!m_phasorEnable)
		return MemReadFloatingBus(nExecutedCycles);

	UINT bits = (UINT) m_phasorMode;
	if (nAddr & 8)
		bits = 0;
	bits |= (nAddr & 7);
	m_phasorMode = (PHASOR_MODE) bits;

	if (m_phasorMode == PH_Mockingboard || m_phasorMode == PH_EchoPlus)
		m_phasorClockScaleFactor = 1;
	else if (m_phasorMode == PH_Phasor)
		m_phasorClockScaleFactor = 2;

	AY8910_InitClock((int)(Get6502BaseClock() * m_phasorClockScaleFactor));

	for (UINT i = 0; i < NUM_SSI263; i++)
		m_MBSubUnit[i].ssi263.SetCardMode(m_phasorMode);

#if DBG_SUPPORT_ECHOPLUS
	if (m_phasorMode == PH_EchoPlus && (nAddr & 0xf) == 0)
		return 0x1f;	// for TMS5220 detection
#endif

	return MemReadFloatingBus(nExecutedCycles);
}

//-----------------------------------------------------------------------------

void MockingboardCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	if (QueryType() == CT_MockingboardC)
		RegisterIoHandler(m_slot, IO_Null, IO_Null, IORead, IOWrite, this, NULL);
	else	// Phasor
		RegisterIoHandler(m_slot, PhasorIO, PhasorIO, IORead, IOWrite, this, NULL);

	if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
		return;

#ifdef NO_DIRECT_X
#else // NO_DIRECT_X
	for (UINT i = 0; i < NUM_SSI263; i++)
	{
		if (!m_MBSubUnit[i].ssi263.DSInit())
			break;
	}
#endif // NO_DIRECT_X
}

//-----------------------------------------------------------------------------

void MockingboardCard::MuteControl(bool mute)
{
	if (mute)
	{
		for (UINT i = 0; i < NUM_SSI263; i++)
			m_MBSubUnit[i].ssi263.Mute();
	}
	else
	{
		for (UINT i = 0; i < NUM_SSI263; i++)
			m_MBSubUnit[i].ssi263.Unmute();
	}
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG
void MockingboardCard::CheckCumulativeCycles(void)
{
	_ASSERT(m_lastCumulativeCycle == g_nCumulativeCycles);
	m_lastCumulativeCycle = g_nCumulativeCycles;
}
#endif

// Called by: ResetState() and Snapshot_LoadState_v2()
void MockingboardCard::SetCumulativeCycles(void)
{
	m_lastCumulativeCycle = g_nCumulativeCycles;
}

// Called by ContinueExecution() at the end of every execution period (~1000 cycles or ~3 cycles when MODE_STEPPING)
void MockingboardCard::Update(const ULONG executedCycles)
{
	for (UINT i = 0; i < NUM_SSI263; i++)
		m_MBSubUnit[i].ssi263.PeriodicUpdate(executedCycles);
}

//-----------------------------------------------------------------------------

// Called by:
// . CpuExecute() every ~1000 cycles @ 1MHz (or ~3 cycles when MODE_STEPPING)
// . MB_SyncEventCallback() on a TIMER1/2 underflow
// . MB_Read() / MB_Write() (for both normal & full-speed)
void MockingboardCard::UpdateCycles(ULONG executedCycles)
{
	CpuCalcCycles(executedCycles);
	UINT64 uCycles = g_nCumulativeCycles - m_lastCumulativeCycle;
	_ASSERT(uCycles >= 0);
	if (uCycles == 0)
		return;

	m_lastCumulativeCycle = g_nCumulativeCycles;
	_ASSERT(uCycles < 0x10000 || g_nAppMode == MODE_BENCHMARK);
	USHORT nClocks = (USHORT)uCycles;

	for (UINT i = 0; i < NUM_SUBUNITS_PER_MB; i++)
	{
		m_MBSubUnit[i].sy6522.UpdateTimer1(nClocks);
		m_MBSubUnit[i].sy6522.UpdateTimer2(nClocks);
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
	//UpdateCycles(uExecutedCycles);	// Underflow: so keep TIMER1/2 counters in sync
	// Update all MBs, so that m_lastCumulativeCycle remains in sync for all
	GetCardMgr().GetMockingboardCardMgr().UpdateCycles(uExecutedCycles);	// Underflow: so keep TIMER1/2 counters in sync

	MB_SUBUNIT* pMB = &m_MBSubUnit[(id & 0xf) / SY6522::kNumTimersPer6522];

	if ((id & 1) == 0)
	{
		_ASSERT(pMB->sy6522.IsTimer1Active());
		UpdateIFRandIRQ(pMB, 0, SY6522::IxR_TIMER1);
		GetCardMgr().GetMockingboardCardMgr().UpdateSoundBuffer();

		if ((pMB->sy6522.GetReg(SY6522::rACR) & SY6522::ACR_RUNMODE) == SY6522::ACR_RM_FREERUNNING)
		{
			pMB->sy6522.StartTimer1();
			if (pMB->sy6522.IsTimer1IrqDelay())
				return 0x0001;	// T1C=0xFFFF, which is really -1, as there's 1 cycle until underflow occurs
			return pMB->sy6522.GetRegT1C() + SY6522::kExtraTimerCycles;
		}

		// One-shot mode
		// - Phasor's playback code uses one-shot mode

		pMB->sy6522.StopTimer1();
		return 0;			// Don't repeat event
	}
	else
	{
		// NB. Since not calling UpdateSoundBuffer(), then AppleWin doesn't (accurately?) support AY-playback using T2 (which is one-shot only)
		_ASSERT(pMB->sy6522.IsTimer2Active());
		UpdateIFRandIRQ(pMB, 0, SY6522::IxR_TIMER2);

		pMB->sy6522.StopTimer2();	// TIMER2 only runs in one-shot mode
		return 0;			// Don't repeat event
	}
}

//-----------------------------------------------------------------------------

bool MockingboardCard::IsActive(void)
{
	bool isSSI263Active = false;
	for (UINT i = 0; i  <NUM_SSI263; i++)
		isSSI263Active |= m_MBSubUnit[i].ssi263.IsPhonemeActive();

	return m_isActive || isSSI263Active;
}

//-----------------------------------------------------------------------------

void MockingboardCard::SetVolume(DWORD volume, DWORD volumeMax)
{
	for (UINT i  =0; i < NUM_SSI263; i++)
		m_MBSubUnit[i].ssi263.SetVolume(volume, volumeMax);
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

	MB_SUBUNIT* pMB = &m_MBSubUnit[0];

	for (UINT i=0; i<MB_UNITS_PER_CARD_v1; i++)
	{
		// 6522
		pMB->sy6522.GetRegs((BYTE*)&pSS->Unit[i].RegsSY6522);	// continuous 16-byte array

		// AY8913
		for (UINT j=0; j<16; j++)
		{
			pSS->Unit[i].RegsAY8910[j] = AYReadReg(i, 0, j);	// FIXME: also support Phasor's 2nd AY8913
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
// AY8913 interface

BYTE MockingboardCard::AYReadReg(BYTE subunit, BYTE ay, int r)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	return m_MBSubUnit[subunit].ay8913[ay].sound_ay_read(r);
}

void MockingboardCard::_AYWriteReg(BYTE subunit, BYTE ay, int r, int v)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	libspectrum_dword uOffset = (libspectrum_dword)(g_nCumulativeCycles - m_lastAYUpdateCycle);
	m_MBSubUnit[subunit].ay8913[ay].sound_ay_write(r, v, uOffset);
}

void MockingboardCard::AY8910_reset(BYTE subunit, BYTE ay)
{
	// Don't reset the AY CLK, as this is a property of the card (MB/Phasor), not the AY chip
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	m_MBSubUnit[subunit].ay8913[ay].sound_ay_reset();	// Calls: sound_ay_init();
}

void MockingboardCard::AY8910UpdateSetCycles()
{
	m_lastAYUpdateCycle = g_nCumulativeCycles;
}

void MockingboardCard::AY8910Update(BYTE subunit, BYTE ay, INT16** buffer, int nNumSamples)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	AY8910UpdateSetCycles();

	m_MBSubUnit[subunit].ay8913[ay].SetFramesize(nNumSamples);
	m_MBSubUnit[subunit].ay8913[ay].SetSoundBuffers(buffer);
	m_MBSubUnit[subunit].ay8913[ay].sound_frame();
}

void MockingboardCard::AY8910_InitAll(int nClock, int nSampleRate)
{
	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		for (UINT ay = 0; ay < 2; ay++)
		{
			m_MBSubUnit[subunit].ay8913[ay].sound_init(NULL);	// Inits mainly static members (except ay_tick_incr)
			m_MBSubUnit[subunit].ay8913[ay].sound_ay_init();
		}
	}
}

void MockingboardCard::AY8910_InitClock(int nClock)
{
	AY8913::SetCLK((double)nClock);

	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		for (UINT ay = 0; ay < NUM_AY8913_PER_SUBUNIT; ay++)
		{
			m_MBSubUnit[subunit].ay8913[ay].sound_init(NULL);	// Inits mainly static members (except ay_tick_incr)
		}
	}
}

BYTE* MockingboardCard::AY8910_GetRegsPtr(BYTE subunit, BYTE ay)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	return m_MBSubUnit[subunit].ay8913[ay].GetAYRegsPtr();
}

UINT MockingboardCard::AY8910_SaveSnapshot(YamlSaveHelper& yamlSaveHelper, BYTE subunit, BYTE ay, const std::string& suffix)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	m_MBSubUnit[subunit].ay8913[ay].SaveSnapshot(yamlSaveHelper, suffix);
	return 1;
}

UINT MockingboardCard::AY8910_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, BYTE subunit, BYTE ay, const std::string& suffix)
{
	_ASSERT(subunit < NUM_SUBUNITS_PER_MB && ay < NUM_AY8913_PER_SUBUNIT);
	return m_MBSubUnit[subunit].ay8913[ay].LoadSnapshot(yamlLoadHelper, suffix) ? 1 : 0;
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
//    Changed at AppleWin 1.30.8
// 9: Phasor AY's are swapped (means that AppleWin 1.30.10 and 1.30.11 are wrong)
//    Changed at AppleWin 1.30.12
//10: Phasor AY's are ordered correctly
//    Changed at AppleWin 1.30.14
const UINT kUNIT_VERSION = 10;

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

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, m_MBSubUnit[0].ssi263.GetVotraxPhoneme());	// SC01 only in subunit 0

	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		MB_SUBUNIT* pMB = &m_MBSubUnit[subunit];

		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_MB_UNIT, subunit);

		pMB->sy6522.SaveSnapshot(yamlSaveHelper);
		AY8910_SaveSnapshot(yamlSaveHelper, subunit, AY8913_DEVICE_A, std::string(""));
		pMB->ssi263.SaveSnapshot(yamlSaveHelper);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);
	}
}

bool MockingboardCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (m_slot == 0 || m_slot == 3)
		throw std::runtime_error("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::runtime_error("Card: wrong version");

	if (QueryType() == CT_Phasor)
		return Phasor_LoadSnapshot(yamlLoadHelper, version);

	//

	AY8910UpdateSetCycles();

	bool isVotrax = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;
	m_MBSubUnit[0].ssi263.SetVotraxPhoneme(isVotrax);	// SC01 only in subunit 0

	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		MB_SUBUNIT* pMB = &m_MBSubUnit[subunit];

		char szNum[2] = {char('0' + subunit), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::runtime_error("Card: Expected key: " + unit);

		pMB->sy6522.LoadSnapshot(yamlLoadHelper, version);
		UpdateIFRandIRQ(pMB, 0, pMB->sy6522.GetReg(SY6522::rIFR));			// Assert any pending IRQs (GH#677)
		AY8910_LoadSnapshot(yamlLoadHelper, subunit, AY8913_DEVICE_A, std::string(""));
		pMB->ssi263.LoadSnapshot(yamlLoadHelper, PH_Mockingboard, version);	// Pre: SetVotraxPhoneme()

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
	}

	AY8910_InitClock((int)Get6502BaseClock());

	// NB. m_phasorEnable setup in ctor

	return true;
}

void MockingboardCard::Phasor_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardNamePhasor(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	yamlSaveHelper.SaveUint(SS_YAML_KEY_PHASOR_MODE, m_phasorMode);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_VOTRAX_PHONEME, m_MBSubUnit[0].ssi263.GetVotraxPhoneme());	// SC01 only in subunit 0

	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		MB_SUBUNIT* pMB = &m_MBSubUnit[subunit];

		YamlSaveHelper::Label unit(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_PHASOR_UNIT, subunit);

		pMB->sy6522.SaveSnapshot(yamlSaveHelper);
		AY8910_SaveSnapshot(yamlSaveHelper, subunit, AY8913_DEVICE_A, std::string("-A"));
		AY8910_SaveSnapshot(yamlSaveHelper, subunit, AY8913_DEVICE_B, std::string("-B"));
		pMB->ssi263.SaveSnapshot(yamlSaveHelper);

		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE, pMB->state);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_MB_UNIT_STATE_B, pMB->stateB);
		yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_AY_CURR_REG, pMB->nAYCurrentRegister);
	}
}

bool MockingboardCard::Phasor_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
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
	m_phasorMode = (PHASOR_MODE) phasorMode;
	m_phasorClockScaleFactor = (m_phasorMode == PH_Phasor) ? 2 : 1;

	AY8910UpdateSetCycles();

	UINT nDeviceNum = 0;
	MB_SUBUNIT* pMB = &m_MBSubUnit[0];

	bool isVotrax = (version >= 6) ? yamlLoadHelper.LoadBool(SS_YAML_KEY_VOTRAX_PHONEME) :  false;
	m_MBSubUnit[0].ssi263.SetVotraxPhoneme(isVotrax);	// SC01 only in subunit 0

	for (UINT subunit = 0; subunit < NUM_SUBUNITS_PER_MB; subunit++)
	{
		MB_SUBUNIT* pMB = &m_MBSubUnit[subunit];

		char szNum[2] = {char('0' + subunit), 0};
		std::string unit = std::string(SS_YAML_KEY_MB_UNIT) + std::string(szNum);
		if (!yamlLoadHelper.GetSubMap(unit))
			throw std::runtime_error("Card: Expected key: " + unit);

		pMB->sy6522.LoadSnapshot(yamlLoadHelper, version);
		UpdateIFRandIRQ(pMB, 0, pMB->sy6522.GetReg(SY6522::rIFR));			// Assert any pending IRQs (GH#677)
		if (version >= 5 && version <= 8)
		{
			const BYTE phasorDevice = subunit == 0 ? AY8913_DEVICE_B : AY8913_DEVICE_A;
			AY8910_LoadSnapshot(yamlLoadHelper, 0, phasorDevice, std::string("-A"));
			AY8910_LoadSnapshot(yamlLoadHelper, 1, phasorDevice, std::string("-B"));
		}
		else if (version <= 4 || version == 9)
		{
			const BYTE phasorDevice = subunit == 0 ? AY8913_DEVICE_A : AY8913_DEVICE_B;
			AY8910_LoadSnapshot(yamlLoadHelper, 0, phasorDevice, std::string("-A"));
			AY8910_LoadSnapshot(yamlLoadHelper, 1, phasorDevice, std::string("-B"));
		}
		else
		{
			AY8910_LoadSnapshot(yamlLoadHelper, subunit, AY8913_DEVICE_A, std::string("-A"));
			AY8910_LoadSnapshot(yamlLoadHelper, subunit, AY8913_DEVICE_B, std::string("-B"));
		}
		pMB->ssi263.LoadSnapshot(yamlLoadHelper, m_phasorMode, version);	// Pre: SetVotraxPhoneme()

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
	}

	AY8910_InitClock((int)(Get6502BaseClock() * m_phasorClockScaleFactor));

	// NB. m_phasorEnable setup in ctor

	return true;
}
