/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2024, Tom Charlesworth, Michael Pohoreski, Nick Westgate
Copyright (C) 2026, Henri Asseily (henri@asseily.com)

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

/* Description: SSI263 emulation
 *
 * Extra "spec" that's not obvious from the datasheet: (GH#175)
 * . Writes to regs 0,1,2 (and reg3.CTL=1) all de-assert the IRQ (and writes to reg3.CTL=0 and regs 4..7 don't) (GH#1197)
 * . A phoneme will continue playing back infinitely; unless the phoneme is changed or CTL=1.
 *   . NB. if silenced (Amplitude=0) it's still playing.
 *   . The IRQ is set at the end of the phoneme.
 *   . If IRQ is then cleared, a new IRQ will occur when the phoneme completes again (but need to clear IRQ with a write to reg0, 1 or 2, even for Mockingboard-C).
 * . CTL=1 sets "PD" (Power Down / "standby") mode, also set at power-on.
 *   . Registers can still be changed in this mode.
 *   . IRQ de-asserted & D7=0.
 * . CTL=0 brings device out of "PD" mode, the mode will be set to DR1,DR0 and the phoneme P5-P0 will play.
 * . Setting mode to DR1:0 = %00 just disables A/!R (ie. disables interrupts), but otherwise retains the previous DR1:0 mode.
 *   . If an IRQ was previously asserted then to set DR1:0=%00, you must go via CTL=1, which de-asserts the IRQ.
 * . Mockingboard-C: CTRL+RESET is not connected to !PD/!RST pin 18.
 *   . Phasor: TODO: check with a 'scope.
 *   . Phasor: with SSI263 ints disabled & reg0's DR1:0 != %00, then CTRL+RESET will cause SSI263 to enable ints & assert IRQ.
 *     . it's as if the SSI263 does a CTL H->L to pick-up the new DR1:0. (Bug in SSI263? Assume it should remain in PD mode.)
 *     . but if CTL=1, then CTRL+RESET has no effect.
 * . Power-on: PD=1 (so D7=0), reg4 (Filter Freq)=0xFF (other regs are seemingly random?).
 */

#include "StdAfx.h"
#include "SSI263.h"

#include "6522.h"
#include "CardManager.h"
#include "Mockingboard.h"
#include "Core.h"
#include "CPU.h"
#include "Log.h"
#include "Memory.h"
#include "SoundCore.h"

#include "YamlHelper.h"

#define LOG_SSI263 0
#define LOG_SSI263B 0	// Alternate SSI263 logging (use in conjunction with CPU.cpp's LOG_IRQ_TAKEN_AND_RTI)
#define LOG_SC01 0

// SSI263A registers:
#define SSI_DURPHON	0x00
#define SSI_INFLECT	0x01
#define SSI_RATEINF	0x02
#define SSI_CTTRAMP	0x03
#define SSI_FILFREQ	0x04

// Historical SC01 compatibility durations, in 1/22050 second ticks.
static const uint32_t kSpeechTimingRate = 22050;
static const UINT kPhonemeTicks[62] =
{
	2656, 2636, 2594, 2707, 2746, 2630, 2644, 3203,
	2639, 2709, 2593, 2577, 2548, 2705, 2620, 2655,
	2731, 2639, 2735, 2676, 2606, 2633, 2674, 2653,
	2555, 2578, 2635, 2650, 2663, 2595, 2512, 2657,
	2618, 2609, 1012, 899, 2691, 896, 880, 905,
	2575, 3199, 2615, 3373, 2539, 2618, 2603, 2648,
	2642, 2600, 2656, 2574, 2720, 2661, 2638, 2731,
	2643, 2594, 2584, 2643, 2564, 2582
};

//-----------------------------------------------------------------------------

#if LOG_SSI263B
static int ssiRegs[5]={-1,-1,-1,-1,-1};
static int totalDuration_ms = 0;

void SSI_Output()
{
	int ssi0 = ssiRegs[SSI_DURPHON];
	int ssi2 = ssiRegs[SSI_RATEINF];

	LogOutput("SSI: ");
	for (int i = 0; i <= 4; i++)
	{
		std::string r = (ssiRegs[i] >= 0) ? ByteToHexStr(ssiRegs[i]) : "--";
		LogOutput("%s ", r.c_str());
		ssiRegs[i] = -1;
	}

	if (ssi0 != -1 && ssi2 != -1)
	{
		int phonemeDuration_ms = (((16-(ssi2>>4))*4096)/1023) * (4-(ssi0>>6));
		totalDuration_ms += phonemeDuration_ms;
		LogOutput("/ duration = %d (total = %d) ms", phonemeDuration_ms, totalDuration_ms);
	}

	LogOutput("\n");
}
#endif

//-----------------------------------------------------------------------------

UINT64 SSI263::GetLastCumulativeCycles()
{
	return dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(m_slot)).GetLastCumulativeCycles();
}

void SSI263::UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask)
{
	dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(m_slot)).UpdateIFR(nDevice, clr_mask, set_mask);
}

BYTE SSI263::GetPCR(BYTE nDevice)
{
	return dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(m_slot)).GetPCR(nDevice);
}

//-----------------------------------------------------------------------------

BYTE SSI263::Read(ULONG nExecutedCycles)
{
	// Regardless of register, just return inverted A/!R in bit7
	// . inverted "A/!R" is high for REQ (ie. Request, as phoneme nearly complete)
	// NB. this doesn't clear the IRQ

	if (m_type == SSI263Empty)
		return MemReadFloatingBus(nExecutedCycles);

	AdvanceSynthesis();
	CommitResponse();
	return MemReadFloatingBus(m_currentMode.D7, nExecutedCycles);
}

void SSI263::Write(BYTE nReg, BYTE nValue)
{
#if LOG_SSI263B
	_ASSERT(nReg < 5);
	if (nReg>4) nReg=4;
	if (ssiRegs[nReg]>=0) SSI_Output();	// overwriting a reg
	ssiRegs[nReg] = nValue;
#endif

	if (m_type == SSI263Empty)
		return;

	AdvanceSynthesis();
	m_synth.Write(nReg < SSI_FILFREQ ? nReg : SSI_FILFREQ, nValue);

	// SSI263 datasheet is not clear, but a write to DURPHON de-asserts the IRQ and clears D7.
	// . Empirically writes to regs 0,1,2 (and reg3.CTL=1) all de-assert the IRQ (and writes to reg3.CTL=0 and regs 4..7 don't) (GH#1197)
	// NB. The same for Mockingboard as there's no automatic handshake from the 6522 (CA2 isn't connected to the SSI263). So writes to regs 0, 1 or 2 complete the "handshake".
	if (nReg <= SSI_RATEINF)
	{
		m_responsePending = false;
		CpuIrqDeassert(IS_SPEECH);
		m_currentMode.D7 = 0;
	}

	switch(nReg)
	{
	case SSI_DURPHON:
#if LOG_SSI263
		if (g_fh) fprintf(g_fh, "DUR   = 0x%02X, PHON = 0x%02X\n\n", nValue>>6, nValue&PHONEME_MASK);
		LogOutput("DUR   = %d, PHON = 0x%02X\n", nValue>>6, nValue&PHONEME_MASK);
#endif
#if LOG_SSI263B
		SSI_Output();
#endif

		m_durationPhoneme = nValue;
		m_isVotraxPhoneme = false;

		if ((m_ctrlArtAmp & CONTROL_MASK) == 0)
			Play(m_durationPhoneme & PHONEME_MASK);		// Play phoneme when *not* in power-down / standby mode
		break;
	case SSI_INFLECT:
#if LOG_SSI263
		if (g_fh) fprintf(g_fh, "INF   = 0x%02X\n", nValue);
#endif
		m_inflection = nValue;
		break;

	case SSI_RATEINF:
#if LOG_SSI263
		if (g_fh) fprintf(g_fh, "RATE  = 0x%02X, INF = 0x%02X\n", nValue>>4, nValue&0x0F);
#endif
		m_rateInflection = nValue;
		// A RATE write on a slot boundary feeds that reload in the hardware.
		if (m_responseReloaded)
			m_responseTicksRemaining = GetResponsePeriod();
		if (m_durationReloaded)
			m_durationTicksRemaining = GetDurationPeriod();
		break;
	case SSI_CTTRAMP:
#if LOG_SSI263
		if (g_fh) fprintf(g_fh, "CTRL  = %d, ART = 0x%02X, AMP=0x%02X\n", nValue>>7, (nValue&ARTICULATION_MASK)>>4, nValue&AMPLITUDE_MASK);
		//
		{
			bool H2L = (m_ctrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK);
			std::string newMode = StrFormat(" (new mode=%d)", m_durationPhoneme>>6);
			LogOutput("CTRL  = %d->%d, ART = 0x%02X, AMP=0x%02X%s\n", m_ctrlArtAmp>>7, nValue>>7, (nValue&ARTICULATION_MASK)>>4, nValue&AMPLITUDE_MASK, H2L?newMode.c_str() : "");
		}
#endif
#if LOG_SSI263B
		if ( ((m_ctrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK)) || ((nValue&0xF) == 0x0) )	// H->L or amp=0
			SSI_Output();
#endif
		if ((m_ctrlArtAmp & CONTROL_MASK) && !(nValue & CONTROL_MASK))	// H->L
		{
			// NB. Just changed from CTL=1 (power-down) - where IRQ was already de-asserted & D7=0
			// . So CTL H->L never affects IRQ or D7
			SetDeviceModeAndInts();

			// Device out of power down / "standby" mode, so play phoneme
			m_isVotraxPhoneme = false;
			Play(m_durationPhoneme & PHONEME_MASK);
		}

		m_ctrlArtAmp = nValue;

		// "Setting the Control bit (CTL) to a logic one puts the device into Power Down mode..." (SSI263 datasheet)
		// . this silences the phoneme - actually "turns off the excitation sources and analog circuits"
		if (m_ctrlArtAmp & CONTROL_MASK)
		{
			m_responsePending = false;
			CpuIrqDeassert(IS_SPEECH);
			m_currentMode.D7 = 0;
			// NB. Don't call Stop() - mb-audit ("Classic Adventure") cuts at end, Berzap! is choppy
		}
		break;
	case SSI_FILFREQ:	// RegAddr.b2=1 (b1 & b0 are: don't care)
	default:
#if LOG_SSI263
		if (g_fh) fprintf(g_fh, "FFREQ = 0x%02X\n", nValue);
#endif
		m_filterFreq = nValue;
		break;
	}
	CommitResponse();
}

void SSI263::SetDeviceModeAndInts()
{
	if ((m_durationPhoneme & DURATION_MODE_MASK) != MODE_IRQ_DISABLED)
	{
		m_currentMode.function = (m_durationPhoneme & DURATION_MODE_MASK) >> DURATION_MODE_SHIFT;
		m_currentMode.enableInts = 1;
	}
	else
	{
		// "Disables A/!R output only; does not change previous A/!R response" (SSI263 datasheet)
		m_currentMode.enableInts = 0;
	}
	m_synth.SetFunction(m_currentMode.function);
}

//-----------------------------------------------------------------------------

const BYTE SSI263::m_Votrax2SSI263[/*64*/] =
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

void SSI263::Votrax_Write(BYTE value)
{
#if LOG_SC01
	LogOutput("SC01: %02X (= SSI263: %02X)\n", value, m_Votrax2SSI263[value & PHONEME_MASK]);
#endif

	if (!m_hasSC01)
		return;

	AdvanceSynthesis();
	m_responsePending = false;
	m_isVotraxPhoneme = true;
	m_votraxPhoneme = value & PHONEME_MASK;
	ConfigureVotrax();

	// !A/R: Acknowledge receipt of phoneme data (signal goes from high to low)
	UpdateIFR(m_device, SY6522::IxR_VOTRAX, 0);

	// SC01 duration does not depend on pitch or SSI263 registers.
	Play(m_Votrax2SSI263[m_votraxPhoneme]);
}

//-----------------------------------------------------------------------------

void SSI263::Play(unsigned int phoneme)
{
	if (m_dbgFirst)
		m_dbgStartTime = g_nCumulativeCycles;

	m_currentActivePhoneme = phoneme;
	if (!m_isVotraxPhoneme)
	{
		m_phonemeLengthRemaining = 0;
		m_phonemeLeadoutLength = 0;
		StartResponseTiming();
		return;
	}

	// The old timing table used the same duration for PA, E and E1.
	const UINT index = phoneme <= 2 ? 0 : phoneme - 2;
	m_phonemeLengthRemaining = kPhonemeTicks[index];
	m_phonemeLeadoutLength = m_phonemeLengthRemaining / 10;
	m_currSampleMod4 = 0;
}

void SSI263::Stop()
{
	if (SSI263SingleVoice.lpDSBvoice && SSI263SingleVoice.bActive)
		DSVoiceStop(&SSI263SingleVoice);
}

//-----------------------------------------------------------------------------

void SSI263::ResetSynthesis(bool resetClock)
{
	m_synth.Reset((uint32_t)(Get6502BaseClock() + 0.5));
	ResetResponseTiming();
	if (resetClock)
	{
		m_synthLastCycle = g_nCumulativeCycles;
		m_synthCpuClock = (uint32_t)(g_fCurrentCLK6502 + 0.5);
		m_synthCycleRemainder = 0;
		m_synthSamplePhase = m_synth.GetClockHz();
		m_speechSamplePhase = 0;
	}
	if (resetClock || !m_isVotraxPhoneme)
		m_synthSamples = 0;
	m_synth.Write(SSI_DURPHON, m_durationPhoneme);
	m_synth.Write(SSI_INFLECT, m_inflection);
	m_synth.Write(SSI_RATEINF, m_rateInflection);
	m_synth.Write(SSI_FILFREQ, m_filterFreq);
	m_synth.Write(SSI_CTTRAMP, m_ctrlArtAmp);
}

void SSI263::ResetVotraxSynthesis()
{
	m_votraxSynth.Reset((uint32_t)(Get6502BaseClock() + 0.5));
	// Fixed surrogate voice: about 91 Hz, with immediate pitch and smooth articulation.
	// These are SSI263 model settings, not an SC01 circuit model. Ignore SC01 pitch bits.
	m_votraxSynth.Write(SSI_DURPHON, MODE_PHONEME_IMMEDIATE_INFLECTION);
	m_votraxSynth.Write(SSI_INFLECT, 0x51);
	m_votraxSynth.Write(SSI_RATEINF, 0x88);
	m_votraxSynth.Write(SSI_FILFREQ, 0xE7);
	m_votraxSynthStarted = false;
}

void SSI263::ConfigureVotrax()
{
	if (!m_votraxSynthStarted)
	{
		m_votraxSynth.Write(SSI_CTTRAMP, 0x5C);
		m_votraxSynthStarted = true;
	}
	m_votraxSynth.Write(SSI_DURPHON, m_Votrax2SSI263[m_votraxPhoneme]);
}

void SSI263::AdvanceSynthesis()
{
	const UINT64 currentCycle = GetLastCumulativeCycles();
	const UINT64 elapsedCycles = currentCycle >= m_synthLastCycle ? currentCycle - m_synthLastCycle : 0;
	m_synthLastCycle = currentCycle;
	if (elapsedCycles)
	{
		CommitResponse();
		m_responseReloaded = false;
		m_durationReloaded = false;
	}
	if (m_type == SSI263Empty && !m_votraxSynthStarted)
		return;

	const uint32_t chipClock = (uint32_t)(Get6502BaseClock() + 0.5);
	const uint32_t cpuClock = (uint32_t)(g_fCurrentCLK6502 + 0.5);
	if (chipClock != m_synth.GetClockHz())
	{
		m_synthSamplePhase = (uint32_t)((UINT64)m_synthSamplePhase * chipClock / m_synth.GetClockHz());
		m_synth.SetClock(chipClock);
		m_votraxSynth.SetClock(chipClock);
	}
	if (cpuClock != m_synthCpuClock)
	{
		m_synthCycleRemainder = (uint32_t)((UINT64)m_synthCycleRemainder * cpuClock / m_synthCpuClock);
		m_synthCpuClock = cpuClock;
	}

	// The speed slider changes CPU throughput, not speech pitch.
	const UINT64 scaledCycles = elapsedCycles * chipClock + m_synthCycleRemainder;
	UINT64 ticks = scaledCycles / cpuClock;
	m_synthCycleRemainder = (uint32_t)(scaledCycles % cpuClock);

	while (ticks)
	{
		// Leave a sample due at the endpoint so writes there take effect first.
		if (m_synthSamplePhase >= chipClock)
		{
			m_synthSamplePhase -= chipClock;
			const short nativeSample = m_type != SSI263Empty ? m_synth.GetSample() : 0;
			const short votraxSample = m_votraxSynthStarted ? m_votraxSynth.GetSample() : 0;
			// PA0, PA1 and STOP retain the old silent output and IRQ timing.
			const short sample = m_isVotraxPhoneme
				? (m_Votrax2SSI263[m_votraxPhoneme] ? votraxSample : 0) : nativeSample;
			m_speechSamplePhase += kSpeechTimingRate;
			if (m_speechSamplePhase >= SSI263Synth::kSampleRate)
			{
				m_speechSamplePhase -= SSI263Synth::kSampleRate;
				AdvanceSpeechTiming();
			}

			if (!g_bFullSpeed && !g_bDisableDirectSound && !g_bDisableDirectSoundMockingboard && IsPhonemeActive())
			{
				// A stalled audio device must not stop the emulated chip.
				if (m_synthSamples == MAX_SAMPLES)
					m_synthSamples = 0;
				// Raise speech by 20 dB for AppleWin's mixer; keep the chip's gain unchanged.
				const int output = sample * 10;
				m_synthBuffer[m_synthSamples++] = (short)(output < -32768 ? -32768 : output > 32767 ? 32767 : output);
			}
		}

		const uint32_t toSample = (chipClock - m_synthSamplePhase + SSI263Synth::kSampleRate - 1) / SSI263Synth::kSampleRate;
		const uint32_t step = ticks < toSample ? (uint32_t)ticks : toSample;
		if (m_type != SSI263Empty)
		{
			m_synth.Advance(step);
			AdvanceResponseTiming(step);
		}
		if (m_votraxSynthStarted)
			m_votraxSynth.Advance(step);
		ticks -= step;
		m_synthSamplePhase += step * SSI263Synth::kSampleRate;
	}

	if (m_synthCycleRemainder)
	{
		// The last XCK edge preceded this CPU boundary, so a later write cannot win it.
		CommitResponse();
		m_responseReloaded = false;
		m_durationReloaded = false;
	}
}

void SSI263::ResetResponseTiming()
{
	m_responseActive = false;
	m_responsePending = false;
	m_responseReloaded = false;
	m_durationReloaded = false;
	m_responseTicksRemaining = 0;
	m_durationTicksRemaining = 0;
	m_responsePhase = 0;
	m_durationPhase = 0;
}

void SSI263::StartResponseTiming()
{
	m_responseActive = true;
	m_responsePending = false;
	m_responseReloaded = false;
	m_durationReloaded = false;
	m_responseTicksRemaining = GetResponsePeriod();
	m_durationTicksRemaining = GetDurationPeriod();
	m_responsePhase = 0;
	m_durationPhase = 0;
}

void SSI263::CommitResponse()
{
	if (!m_responsePending)
		return;
	m_responsePending = false;
	if (!m_isVotraxPhoneme)
		SetSpeechIRQ();
}

void SSI263::AdvanceResponseTiming(uint32_t ticks)
{
	if (!m_responseActive)
		return;

	// Match the FPGA response counters: 16 slots per request. RATE is live
	// at each reload; R1/R2 acknowledge a request without restarting the slots.
	while (ticks)
	{
		CommitResponse();
		m_responseReloaded = false;
		m_durationReloaded = false;
		UINT step = ticks;
		if (step > m_responseTicksRemaining)
			step = m_responseTicksRemaining;
		if (step > m_durationTicksRemaining)
			step = m_durationTicksRemaining;
		ticks -= step;
		m_responseTicksRemaining -= step;
		m_durationTicksRemaining -= step;

		if (!m_responseTicksRemaining)
		{
			m_responseTicksRemaining = GetResponsePeriod();
			m_responseReloaded = true;
			m_responsePhase = (m_responsePhase + 1) & 15;
			if (!m_responsePhase && m_currentMode.function == 1)
				m_responsePending = true;
		}
		if (!m_durationTicksRemaining)
		{
			m_durationTicksRemaining = GetDurationPeriod();
			m_durationReloaded = true;
			m_durationPhase = (m_durationPhase + 1) & 15;
			if (!m_durationPhase && (m_currentMode.function == 2 || m_currentMode.function == 3))
				m_responsePending = true;
		}
	}
	// Keep a boundary request pending until the bus operation is known.
	// An ACK on this same cycle must not latch an interrupt in the VIA.
}

void SSI263::AdvanceSpeechTiming()
{
	if (!m_isVotraxPhoneme || !IsPhonemeActive())
		return;

	// SC01 keeps its existing compatibility timing; native SSI uses response counters.
	if (m_phonemeLengthRemaining)
	{
		m_currSampleMod4 = (m_currSampleMod4 + 1) & 3;
		m_phonemeLengthRemaining--;
		if (!m_phonemeLengthRemaining)
			UpdateIRQ();
	}
	else if (m_phonemeLeadoutLength)
	{
		if (--m_phonemeLeadoutLength == 0)
			RepeatPhoneme();
	}
}

void SSI263::UpdateSynthesis()
{
	const UINT availableSamples = m_synthSamples;
	m_synthSamples = 0;
	if (!IsPhonemeActive() || g_bFullSpeed || !DSInit())
	{
		m_byteOffset = (uint32_t)-1;
		return;
	}

	DWORD playCursor, writeCursor;
	if (FAILED(SSI263SingleVoice.lpDSBvoice->GetCurrentPosition(&playCursor, &writeCursor)))
		return;

	const UINT targetBytes = m_kDSBufferByteSize / 4;
	const bool prefill = m_byteOffset == (uint32_t)-1;
	if (prefill)
	{
		m_byteOffset = writeCursor;
		m_numSamplesError = 0;
	}
	else if (SoundCore_ValidateAndAlignWriteOffset(m_byteOffset, playCursor, writeCursor))
	{
		m_numSamplesError = 0;
	}

	UINT sampleCount = targetBytes / sizeof(short);
	if (prefill)
	{
		memset(m_mixBufferSSI263, 0, sampleCount * sizeof(short));
		m_synthSamples = availableSamples;	// Queue the first audio chunk after the silence.
	}
	else
	{
		if (!availableSamples)
			return;
		const UINT queuedBytes = (m_byteOffset + m_kDSBufferByteSize - playCursor) % m_kDSBufferByteSize;
		// Limit clock drift correction to 0.5%. Carry the fraction between chunks.
		if (queuedBytes < targetBytes)
			m_numSamplesError += (int)availableSamples;
		else if (queuedBytes > m_kDSBufferByteSize / 2)
			m_numSamplesError -= (int)availableSamples;
		else
			m_numSamplesError = 0;
		sampleCount = (UINT)((int)availableSamples + m_numSamplesError / 200);
		m_numSamplesError %= 200;
		if (sampleCount > MAX_SAMPLES)
			sampleCount = MAX_SAMPLES;

		// Buffer correction resamples completed audio; it never clocks the synth.
		if (sampleCount == availableSamples)
			memcpy(m_mixBufferSSI263, m_synthBuffer, sampleCount * sizeof(short));
		else if (sampleCount == 1)
			m_mixBufferSSI263[0] = m_synthBuffer[0];
		else if (sampleCount > 1)
		{
			// Keep both endpoints so the next chunk starts at the next source sample.
			const UINT span = sampleCount - 1;
			for (UINT i = 0; i < span; i++)
			{
				const UINT64 position = (UINT64)i * (availableSamples - 1);
				const UINT source = (UINT)(position / span);
				const UINT fraction = (UINT)(position % span);
				const int first = m_synthBuffer[source];
				const int next = source + 1 < availableSamples ? m_synthBuffer[source + 1] : first;
				m_mixBufferSSI263[i] = (short)(first + (next - first) * (int)fraction / (int)span);
			}
			m_mixBufferSSI263[span] = m_synthBuffer[availableSamples - 1];
		}
	}

	if (!sampleCount)
		return;

	DWORD firstSize, secondSize;
	short *firstBuffer, *secondBuffer;
	if (FAILED(DSGetLock(SSI263SingleVoice.lpDSBvoice, m_byteOffset, sampleCount * sizeof(short),
		&firstBuffer, &firstSize, &secondBuffer, &secondSize)))
		return;
	memcpy(firstBuffer, m_mixBufferSSI263, firstSize);
	if (secondBuffer)
		memcpy(secondBuffer, m_mixBufferSSI263 + firstSize / sizeof(short), secondSize);
	if (SUCCEEDED(SSI263SingleVoice.lpDSBvoice->Unlock(firstBuffer, firstSize, secondBuffer, secondSize)))
		m_byteOffset = (m_byteOffset + sampleCount * sizeof(short)) % m_kDSBufferByteSize;
}

//-----------------------------------------------------------------------------

void SSI263::PeriodicUpdate(UINT executedCycles)
{
	AdvanceSynthesis();
	CommitResponse();
	const UINT kCyclesPerAudioFrame = 1000;
	m_cyclesThisAudioFrame += executedCycles;
	if (m_cyclesThisAudioFrame < kCyclesPerAudioFrame)
		return;

	m_cyclesThisAudioFrame %= kCyclesPerAudioFrame;

	Update();
}

//-----------------------------------------------------------------------------

// Called by:
// . PeriodicUpdate()
void SSI263::Update()
{
	AdvanceSynthesis();
	CommitResponse();
	UpdateSynthesis();
}

// Called by:
// . Update()
// . LoadSnapshot()
void SSI263::RepeatPhoneme()
{
	if (m_phonemeLeadoutLength != 0)
		return;

	if (!m_isVotraxPhoneme)
	{
		if ((m_ctrlArtAmp & CONTROL_MASK) == 0)
		{
			// Only check _ASSERT when SSI263.CONTROL=0 (otherwise kPhonemeLeadoutFlag will be clear 2nd time Update() is called)
			_ASSERT(m_currentActivePhoneme & kPhonemeLeadoutFlag);
			Play(m_durationPhoneme & PHONEME_MASK);		// Repeat this phoneme again
		}

		m_currentActivePhoneme &= PHONEME_MASK;			// Clear kPhonemeLeadoutFlag
	}
	else
	{
		Play(m_Votrax2SSI263[m_votraxPhoneme]);		// Votrax phoneme repeats too (tested in MAME 0.262)
	}
}

//-----------------------------------------------------------------------------

// Complete the current compatibility timing period.
void SSI263::UpdateIRQ()
{
	m_phonemeLengthRemaining = 0;

	_ASSERT(m_currentActivePhoneme != -1);
	_ASSERT((m_currentActivePhoneme & kPhonemeLeadoutFlag) == 0);
	m_currentActivePhoneme |= kPhonemeLeadoutFlag;

	if (m_dbgFirst && m_dbgStartTime)
	{
#if LOG_SSI263 || LOG_SSI263B || LOG_SC01
		UINT64 diff = g_nCumulativeCycles - m_dbgStartTime;
		LogOutput("1st phoneme playback time = 0x%08X cy\n", (UINT32)diff);
#endif
		m_dbgFirst = false;
	}

	// Phoneme complete, so generate IRQ if necessary
	SetSpeechIRQ();
}

//-----------------------------------------------------------------------------

// Pre: m_isVotraxPhoneme, m_cardMode, m_device
void SSI263::SetSpeechIRQ()
{
	if (!m_isVotraxPhoneme && (m_ctrlArtAmp & CONTROL_MASK) == 0)
	{
		if (m_currentMode.enableInts)
		{
			if (m_cardMode == PH_Mockingboard)
			{
				if (m_currentMode.D7 == 0)
				{
					// 6522's PCR = 0x0C (all SSI263 speech routine use this value, but 0x00 will do equally as well!)
					// . b3:1 CA2 Control = b#110 (Low output) - not connected
					// . b0   CA1 Latch/Input = 0 (Negative active edge) - input from SSI263's A/!R
					if ((GetPCR(m_device) & 1) == 0)		// Level change from SSI263's A/!R, latch this as an interrupt
						UpdateIFR(m_device, 0, SY6522::IxR_SSI263);
				}
			}
			else if (m_cardMode == PH_Phasor)
			{
				// Phasor (in native mode): SSI263 IRQ (A/!R) pin is connected directly to the 6502's IRQ
				// . And Mockingboard mode: A/!R is connected to the 6522's CA1
				CpuIrqAssert(IS_SPEECH);
			}
			else
			{
				_ASSERT(m_cardMode == PH_EchoPlus);
				// SSI263 not visible from Echo+ mode, but still continues to operate
			}
		}

		// Always set SSI263's D7 pin regardless of SSI263 mode (DR1:0), including when SSI263 ints are disabled (via MODE_IRQ_DISABLED)
		// NB. Don't set D7 when in power-down / standby mode.
		m_currentMode.D7 = 1;
	}

	//

	if (m_isVotraxPhoneme && GetPCR(m_device) == 0xB0)
	{
		// !A/R: Time-out of old phoneme (signal goes from low to high)
		UpdateIFR(m_device, 0, SY6522::IxR_VOTRAX);
	}
}

//-----------------------------------------------------------------------------

void SSI263::SetCardMode(PHASOR_MODE mode)
{
	const PHASOR_MODE oldCardMode = m_cardMode;
	m_cardMode = mode;

	if (oldCardMode == m_cardMode)
		return;

	// mode change

	if (m_currentMode.D7 == 1)
	{
		m_currentMode.D7 = 0;	// So that \PH_Mockingboard\ path sets IFR. Post: D7=1
		SetSpeechIRQ();
	}

	if (m_cardMode != PH_Phasor)
		CpuIrqDeassert(IS_SPEECH);
}

//-----------------------------------------------------------------------------

// Cf. void MockingboardCardManager::UpdateSoundBuffer()
bool SSI263::DSInit()
{
	if (!SSI263SingleVoice.lpDSBvoice)
	{
		if (g_bDisableDirectSound || g_bDisableDirectSoundMockingboard)
			return false;

		if (!Init())
			return false;
	}

	if (!SSI263SingleVoice.bActive)
	{
		bool bRes = DSZeroVoiceBuffer(&SSI263SingleVoice, m_kDSBufferByteSize);	// ... and Play()
		LogFileOutput("SSI263: DSZeroVoiceBuffer(), res=%d\n", bRes ? 1 : 0);
		if (!bRes)
			return false;
	}

	return true;
}

// Cf. bool MockingboardCardManager::Init()
bool SSI263::Init()
{
	if (!DSAvailable())
		return false;

	HRESULT hr = DSGetSoundBuffer(&SSI263SingleVoice, m_kDSBufferByteSize, SSI263Synth::kSampleRate, m_kNumChannels, "SSI263");
	LogFileOutput("SSI263: DSGetSoundBuffer(), hr=0x%08X\n", hr);
	if (FAILED(hr))
	{
		LogFileOutput("SSI263: DSGetSoundBuffer failed (%08X)\n", hr);
		return false;
	}

	// Don't DirectSoundBuffer::Play() via DSZeroVoiceBuffer() - instead wait until this SSI263 is actually first used
	// . different to Speaker & Mockingboard ring buffers
	// . NB. we have 2x SSI263 per MB card, and it's rare if 1 is used (and *extremely* rare if 2 are used!)
	// . Not so rare, as TotalReplay (at boot) will try to detect an SSI263 (by playing a $00 phoneme).

	hr = SSI263SingleVoice.lpDSBvoice->SetVolume(SSI263SingleVoice.nVolume);
	LogFileOutput("SSI263::DSInit: SetVolume(), hr=0x%08X\n", hr);

	return true;
}

void SSI263::DSUninit()
{
	Stop();
	DSReleaseSoundBuffer(&SSI263SingleVoice);
}

//-----------------------------------------------------------------------------

// MB-C/Phasor/SSI263P phoneme continues to play after CTRL+RESET, whereas Phasor/SSI263AP doesn't (tested on real h/w)
// Votrax phoneme continues to play after CTRL+RESET (tested on MAME 0.262)
void SSI263::Reset(const bool powerCycle, const bool isPhasorCard)
{
	if (m_type == SSI263Empty && !m_hasSC01)
		return;

	// Power-on reset can run before the card has a slot entry.
	if (!powerCycle)
		AdvanceSynthesis();
	m_responsePending = false;

	if (!powerCycle && m_type == SSI263Empty)
		return;

	if (!powerCycle && m_type == SSI263P)
	{
		if (isPhasorCard)
		{
			// [SSI263P] Empirically observed it does CTL H->L to enable ints (and set the device mode?) (GH#175)
			// NB. CTRL+RESET doesn't clear m_ctrlArtAmp.CTL (ie. if the device is in power-down/standby mode then ignore RST)
			// There's a bug in the SSI263P and RST should put the device into power-down/standby mode (ie. silence the device)
			// TODO: Stick a 'scope on !PD/!RST pin 18 to see what the Phasor h/w does.
			if ((m_ctrlArtAmp & CONTROL_MASK) == 0)
				SetDeviceModeAndInts();
		}

		return;
	}

	if (powerCycle || !m_isVotraxPhoneme)
		Stop();
	ResetState(powerCycle);
	CpuIrqDeassert(IS_SPEECH);
}

//-----------------------------------------------------------------------------

void SSI263::Mute()
{
	if (SSI263SingleVoice.bActive && !SSI263SingleVoice.bMute)
	{
		SSI263SingleVoice.lpDSBvoice->SetVolume(DSBVOLUME_MIN);
		SSI263SingleVoice.bMute = true;
	}
}

void SSI263::Unmute()
{
	if (SSI263SingleVoice.bActive && SSI263SingleVoice.bMute)
	{
		SSI263SingleVoice.lpDSBvoice->SetVolume(SSI263SingleVoice.nVolume);
		SSI263SingleVoice.bMute = false;
	}
}

void SSI263::SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax)
{
	SSI263SingleVoice.dwUserVolume = dwVolume;

	SSI263SingleVoice.nVolume = NewVolume(dwVolume, dwVolumeMax);

	if (SSI263SingleVoice.bActive && !SSI263SingleVoice.bMute)
		SSI263SingleVoice.lpDSBvoice->SetVolume(SSI263SingleVoice.nVolume);
}

//=============================================================================

#define SS_YAML_KEY_SSI263 "SSI263"
// NB. No version - this is determined by the parent "Mockingboard C" or "Phasor" unit

#define SS_YAML_KEY_SSI263_REG_DUR_PHON "Duration / Phoneme"
#define SS_YAML_KEY_SSI263_REG_INF "Inflection"
#define SS_YAML_KEY_SSI263_REG_RATE_INF "Rate / Inflection"
#define SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP "Control / Articulation / Amplitude"
#define SS_YAML_KEY_SSI263_REG_FILTER_FREQ "Filter Frequency"
#define SS_YAML_KEY_SSI263_CURRENT_MODE "Current Mode"
#define SS_YAML_KEY_SSI263_ACTIVE_PHONEME "Active Phoneme"	// v13: deprecated
#define SS_YAML_KEY_SSI263_TYPE "Type"	// v14

#define TYPE_SSI263_EMPTY "Empty"
#define TYPE_SSI263_P "SSI263P"
#define TYPE_SSI263_AP "SSI263AP"

void SSI263::SaveSynthesis(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "Synthesis:\n");
	yamlSaveHelper.SaveUint("CPU Clock", m_synthCpuClock);
	yamlSaveHelper.SaveUint("Cycle Remainder", m_synthCycleRemainder);
	yamlSaveHelper.SaveUint("Sample Phase", m_synthSamplePhase);
	yamlSaveHelper.SaveUint("Speech Sample Phase", m_speechSamplePhase);
	yamlSaveHelper.SaveInt("Active Phoneme", m_currentActivePhoneme);
	yamlSaveHelper.SaveUint("Phoneme Remaining", m_phonemeLengthRemaining);
	yamlSaveHelper.SaveUint("Leadout Remaining", m_phonemeLeadoutLength);
	yamlSaveHelper.SaveUint("Sample Modulo", m_currSampleMod4);

	{
		const std::vector<uint32_t> words = m_synth.SaveState();
		YamlSaveHelper::Label state(yamlSaveHelper, "Core State:\n");
		for (size_t i = 0; i < words.size(); i++)
			yamlSaveHelper.SaveHexUint32(StrFormat("%u", (UINT)i).c_str(), words[i]);
	}

	YamlSaveHelper::Label timing(yamlSaveHelper, "Response Timing:\n");
	yamlSaveHelper.SaveBool("Active", m_responseActive);
	yamlSaveHelper.SaveBool("Pending", m_responsePending);
	yamlSaveHelper.SaveBool("Response Reloaded", m_responseReloaded);
	yamlSaveHelper.SaveBool("Duration Reloaded", m_durationReloaded);
	yamlSaveHelper.SaveUint("Response Remaining", m_responseTicksRemaining);
	yamlSaveHelper.SaveUint("Duration Remaining", m_durationTicksRemaining);
	yamlSaveHelper.SaveUint("Response Phase", m_responsePhase);
	yamlSaveHelper.SaveUint("Duration Phase", m_durationPhase);
}

void SSI263::LoadSynthesis(YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap("Synthesis"))
		throw std::runtime_error("SSI263: Expected synthesis state");
	const uint32_t cpuClock = yamlLoadHelper.LoadUint("CPU Clock");
	const uint32_t cycleRemainder = yamlLoadHelper.LoadUint("Cycle Remainder");
	const uint32_t samplePhase = yamlLoadHelper.LoadUint("Sample Phase");
	const uint32_t speechSamplePhase = yamlLoadHelper.LoadUint("Speech Sample Phase");
	const int activePhoneme = yamlLoadHelper.LoadInt("Active Phoneme");
	const UINT phonemeRemaining = yamlLoadHelper.LoadUint("Phoneme Remaining");
	const UINT leadoutRemaining = yamlLoadHelper.LoadUint("Leadout Remaining");
	const UINT sampleModulo = yamlLoadHelper.LoadUint("Sample Modulo");

	if (!yamlLoadHelper.GetSubMap("Core State"))
		throw std::runtime_error("SSI263: Expected core state");
	std::vector<uint32_t> words = m_synth.SaveState();
	for (size_t i = 0; i < words.size(); i++)
		words[i] = yamlLoadHelper.LoadUint(StrFormat("%u", (UINT)i));
	SSI263Synth synth;
	if (!synth.LoadState(words) || !cpuClock || cycleRemainder >= cpuClock
		|| samplePhase >= synth.GetClockHz() + SSI263Synth::kSampleRate || speechSamplePhase >= SSI263Synth::kSampleRate
		|| activePhoneme < -1 || activePhoneme > (int)(kPhonemeLeadoutFlag | PHONEME_MASK)
		|| sampleModulo > 3)
		throw std::runtime_error("SSI263: Invalid synthesis state");
	yamlLoadHelper.PopMap();

	// Earlier candidate snapshots did not save native response counters.
	bool responseActive = m_type != SSI263Empty && activePhoneme >= 0 && !(m_ctrlArtAmp & CONTROL_MASK);
	bool responsePending = false;
	bool responseReloaded = false;
	bool durationReloaded = false;
	UINT responseRemaining = responseActive ? GetResponsePeriod() : 0;
	UINT durationRemaining = responseActive ? GetDurationPeriod() : 0;
	UINT responsePhase = 0;
	UINT durationPhase = 0;
	if (yamlLoadHelper.GetSubMap("Response Timing"))
	{
		responseActive = yamlLoadHelper.LoadBool("Active");
		responsePending = yamlLoadHelper.LoadBool("Pending");
		responseReloaded = yamlLoadHelper.LoadBool("Response Reloaded");
		durationReloaded = yamlLoadHelper.LoadBool("Duration Reloaded");
		responseRemaining = yamlLoadHelper.LoadUint("Response Remaining");
		durationRemaining = yamlLoadHelper.LoadUint("Duration Remaining");
		responsePhase = yamlLoadHelper.LoadUint("Response Phase");
		durationPhase = yamlLoadHelper.LoadUint("Duration Phase");
		if (responsePhase > 15 || durationPhase > 15 || responseRemaining > 4096 || durationRemaining > 16384
			|| (responseActive && (!responseRemaining || !durationRemaining))
			|| (!responseActive && (responsePending || responseReloaded || durationReloaded || responseRemaining || durationRemaining)))
			throw std::runtime_error("SSI263: Invalid response timing");
		yamlLoadHelper.PopMap();
	}
	yamlLoadHelper.PopMap();

	m_synth = synth;
	m_synthCpuClock = cpuClock;
	m_synthCycleRemainder = cycleRemainder;
	m_synthSamplePhase = samplePhase;
	m_speechSamplePhase = speechSamplePhase;
	m_currentActivePhoneme = activePhoneme;
	m_phonemeLengthRemaining = phonemeRemaining;
	m_phonemeLeadoutLength = leadoutRemaining;
	m_currSampleMod4 = sampleModulo;
	m_responseActive = responseActive;
	m_responsePending = responsePending;
	m_responseReloaded = responseReloaded;
	m_durationReloaded = durationReloaded;
	m_responseTicksRemaining = responseRemaining;
	m_durationTicksRemaining = durationRemaining;
	m_responsePhase = (BYTE)responsePhase;
	m_durationPhase = (BYTE)durationPhase;
}

void SSI263::SaveSnapshot(YamlSaveHelper& yamlSaveHelper, UINT subunit)
{
	AdvanceSynthesis();
	CommitResponse();
	// Scope for SSI263 subunit (so that SC01 has same indentation)
	{
		YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SSI263);

		std::string type = m_type == SSI263Empty ? TYPE_SSI263_EMPTY
			: m_type == SSI263P ? TYPE_SSI263_P
			: TYPE_SSI263_AP;

		yamlSaveHelper.SaveString(SS_YAML_KEY_SSI263_TYPE, type);

		if (m_type != SSI263Empty)
		{
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_DUR_PHON, m_durationPhoneme);
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_INF, m_inflection);
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_RATE_INF, m_rateInflection);
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP, m_ctrlArtAmp);
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_REG_FILTER_FREQ, m_filterFreq);
			yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SSI263_CURRENT_MODE, m_currentMode.mode);
		}
		SaveSynthesis(yamlSaveHelper);
	}

	if (subunit == 0)	// has SC01
		SC01_SaveSnapshot(yamlSaveHelper);
}

void SSI263::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, PHASOR_MODE mode, UINT version, UINT subunit)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_SSI263))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_SSI263);

	std::string type = TYPE_SSI263_P;	// Default prior to v14
	if (version >= 14)
		type = yamlLoadHelper.LoadString(SS_YAML_KEY_SSI263_TYPE);

	if (type == TYPE_SSI263_EMPTY)
		m_type = SSI263Empty;
	else if (type == TYPE_SSI263_P)
		m_type = SSI263P;
	else // TYPE_SSI263_AP
		m_type = SSI263AP;

	if (m_type != SSI263Empty)
	{
		m_durationPhoneme = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_DUR_PHON);
		m_inflection = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_INF);
		m_rateInflection = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_RATE_INF);
		m_ctrlArtAmp = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_CTRL_ART_AMP);
		m_filterFreq = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_REG_FILTER_FREQ);
		m_currentMode.mode = yamlLoadHelper.LoadUint(SS_YAML_KEY_SSI263_CURRENT_MODE);

		if (version >= 7 && version < 13)
			yamlLoadHelper.LoadBool(SS_YAML_KEY_SSI263_ACTIVE_PHONEME);	// Consume redundant data

		if (version < 12)
		{
			if (m_currentMode.function == 0)	// invalid function (but in older versions this was accepted)
			{
				m_currentMode.function = MODE_PHONEME_TRANSITIONED_INFLECTION >> DURATION_MODE_SHIFT;	// Typically this is used
				m_currentMode.enableInts = 0;
			}
			else
			{
				m_currentMode.enableInts = 1;
			}
		}

		if ((m_ctrlArtAmp & CONTROL_MASK) == 0)
			m_currentActivePhoneme = m_durationPhoneme & PHONEME_MASK;
	}

	if (version >= 16)
		LoadSynthesis(yamlLoadHelper);
	else
	{
		// Older snapshots only stored registers, so start a fresh synth voice.
		ResetSynthesis();
		m_synth.SetFunction(m_currentMode.function);
	}

	yamlLoadHelper.PopMap();

	//

	_ASSERT(m_device != BYTE(-1));
	SetCardMode(mode);

	// Only need to directly assert IRQ for Phasor mode (for Mockingboard mode it's done via UpdateIFR() in parent)
	if (m_cardMode == PH_Phasor && IsPhonemeActive() && m_currentMode.enableInts && m_currentMode.D7 == 1)
		CpuIrqAssert(IS_SPEECH);

	if (subunit == 0)	// has SC01
		SC01_LoadSnapshot(yamlLoadHelper, version);

	if (version < 16 && (m_isVotraxPhoneme || IsPhonemeActive()))
	{
		// Old snapshots lack timing state. Keep their completion-and-restart policy.
		m_currentActivePhoneme = 0;
		UpdateIRQ();
		RepeatPhoneme();
	}

	m_synthLastCycle = GetLastCumulativeCycles();
	m_synthSamples = 0;
	m_byteOffset = (uint32_t)-1;
}

//=============================================================================

#define SS_YAML_KEY_SC01 "SC01"
// NB. No version - this is determined by the parent "Mockingboard C" or "Phasor" unit

#define SS_YAML_KEY_SC01_PHONEME "SC01 Phoneme"
#define SS_YAML_KEY_SC01_ACTIVE_PHONEME "SC01 Active Phoneme"
#define SS_YAML_KEY_SC01_TYPE "Type"	// v14

#define TYPE_SC01_EMPTY "Empty"
#define TYPE_SC01 "SC01"

void SSI263::SC01_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SC01);

	std::string type = !m_hasSC01 ? TYPE_SC01_EMPTY : TYPE_SC01;
	yamlSaveHelper.SaveString(SS_YAML_KEY_SC01_TYPE, type);

	if (m_hasSC01)
	{
		yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SC01_PHONEME, m_votraxPhoneme);
		yamlSaveHelper.SaveBool(SS_YAML_KEY_SC01_ACTIVE_PHONEME, m_isVotraxPhoneme);
		YamlSaveHelper::Label state(yamlSaveHelper, "Synthesis:\n");
		yamlSaveHelper.SaveBool("Started", m_votraxSynthStarted);
		const std::vector<uint32_t> words = m_votraxSynth.SaveState();
		YamlSaveHelper::Label core(yamlSaveHelper, "Core State:\n");
		for (size_t i = 0; i < words.size(); i++)
			yamlSaveHelper.SaveHexUint32(StrFormat("%u", (UINT)i).c_str(), words[i]);
	}
}

void SSI263::SC01_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	ResetVotraxSynthesis();
	if (version < 12)
	{
		m_votraxPhoneme = 0;
		// The parent sets the active flag for these older snapshots.
		if (m_isVotraxPhoneme)
			ConfigureVotrax();
		return;
	}

	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_SC01))
		throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_SC01);

	std::string type = TYPE_SC01;
	if (version >= 14)
		type = yamlLoadHelper.LoadString(SS_YAML_KEY_SC01_TYPE);
	m_hasSC01 = (type == TYPE_SC01);

	if (m_hasSC01)
	{
		m_votraxPhoneme = yamlLoadHelper.LoadUint(SS_YAML_KEY_SC01_PHONEME);
		m_isVotraxPhoneme = yamlLoadHelper.LoadBool(SS_YAML_KEY_SC01_ACTIVE_PHONEME);
		if (m_votraxPhoneme > PHONEME_MASK)
			throw std::runtime_error("SC01: Invalid phoneme");

		if (version >= 16 && yamlLoadHelper.GetSubMap("Synthesis"))
		{
			const bool started = yamlLoadHelper.LoadBool("Started");
			if (!yamlLoadHelper.GetSubMap("Core State"))
				throw std::runtime_error("SC01: Expected core state");
			std::vector<uint32_t> words = m_votraxSynth.SaveState();
			for (size_t i = 0; i < words.size(); i++)
				words[i] = yamlLoadHelper.LoadUint(StrFormat("%u", (UINT)i));
			if (!m_votraxSynth.LoadState(words) || (m_isVotraxPhoneme && !started))
				throw std::runtime_error("SC01: Invalid synthesis state");
			m_votraxSynthStarted = started;
			yamlLoadHelper.PopMap();
			yamlLoadHelper.PopMap();
		}
		else if (m_isVotraxPhoneme)
		{
			ConfigureVotrax();
		}
	}
	else
	{
		m_isVotraxPhoneme = false;
	}

	yamlLoadHelper.PopMap();
}
