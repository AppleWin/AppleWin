/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2026, Henri Asseily (henri@asseily.com)

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details. You should have received a copy of the GNU General
Public License along with this program; if not, see <https://www.gnu.org/licenses/>.

SSI263 speech synthesis from its parameter ROM and the SC-02 circuit model.
Adapted from the Appletini ONE project at https://github.com/hasseily/appletini-one
*/

#ifndef SSI263_SYNTH_TEST
#include "StdAfx.h"
#endif
#include "SSI263Synth.h"

#include <cassert>
#include <climits>
#include <stdexcept>

// Native SSI-263 source and ideal SC-02 charge-transfer filters.
// See docs/SSI263-synthesis.md for circuit sources and model limits.

const uint32_t SSI263Synth::kSampleRate;

namespace
{
	int And(int a, int b)
	{
		return a == 0 || b == 0 ? 0 : a < 0 || b < 0 ? -1 : 1;
	}

	int Or(int a, int b)
	{
		return a == 1 || b == 1 ? 1 : a < 0 || b < 0 ? -1 : 0;
	}

	int LoadLatch(int oldValue, int enable, int value)
	{
		return enable == 1 ? value : enable == 0 || oldValue == value ? oldValue : -1;
	}

	// SSI-263A active ROM, CRC32 of the original 2 KiB image: CC0A72EE.
	// SHA-256 of these 512 bytes: 101d129a5f104e6190f2eca518bbf9ef65bf4ff92684d29eba56d9641aa02b0a.
	//
	// Layout: 64 native phonemes ($00..$3F), eight bytes per phoneme.
	// Address = (phoneme << 3) | selector. The unused $200..$7FF bytes of
	// the original image are zero. These are circuit parameters, not audio samples.
	// Each byte holds a 4-bit target in bits 7..4 and control flags in bits 3..0.
	// Targets are capacitor-bank codes, not frequencies in Hz or linear gains.
	//
	// Selector   Target in bits 7..4          Control bits used
	//    0       F1                           bit 0: voice transition start
	//    1       F2                           bit 0: fricative transition start
	//    2       F2 resonance (Q)             bits 1..3: source, transition, route
	//    3       Shared F3/F4                 none
	//    4       Filter amplitude             none; ROM byte is zero, use host AMP
	//    5       Voice amplitude              none
	//    6       Fricative (noise) amplitude  none
	//    7       No parameter write           none; ROM byte is zero
	// F5 has fixed components and no ROM target.
	//
	// The control pins are TPARM0..3 in the SC-02 schematic. Their meanings
	// depend on the selector; they are not extra target bits or phoneme classes.
	// At selectors 0/1, bit 0 chooses duration phase 2 (set) or 6 (clear),
	// out of 16 phases. The matching scan sets PW0/PW1, which then permit
	// voice/fricative amplitude steps. A phoneme write clears both latches.
	//
	// At selector 2:
	// - Bit 1 controls PW3. While PW1 permits a load, PW3 becomes
	//   (CTL || !bit1); otherwise it holds. PW3 affects source/envelope timing.
	// - Bit 2 loads PW2 and its inverse PW5. PW5 can block F1, F2 and F3/F4
	//   transition steps; PW2 also helps permit a fricative-route change.
	// - Bit 3 requests the fricative route. Its latch loads only when PW1,
	//   old PW2/current bit 2, and the amplitude-zero conditions permit it.
	//   Separate phase latches then select FRIC1 at the F2 output (bit 3 set)
	//   or FRIC2 after F5 (bit 3 clear). They are not live complements.
	// Selector-2 bit 0 and all low bits at selectors 3..7 are zero in this ROM.
	// For example, HF ($2C) and HFC ($2D) have the same targets; only selector-2
	// bit 1 differs, so their sound differs through source/envelope control.
	const uint8_t phonemeRom[512] =
	{
		0x70, 0x90, 0x0A, 0xC0, 0x00, 0x00, 0x00, 0x00, // $00
		0x20, 0xE1, 0x0E, 0xE0, 0x00, 0xC0, 0x00, 0x00, // $01
		0x50, 0xE1, 0x0E, 0xD0, 0x00, 0xA0, 0x00, 0x00, // $02
		0x10, 0xD1, 0x0E, 0xE0, 0x00, 0xB0, 0x00, 0x00, // $03
		0x20, 0xC1, 0x0E, 0xB0, 0x00, 0x60, 0x00, 0x00, // $04
		0x30, 0xE1, 0x0E, 0xE0, 0x00, 0xC0, 0x00, 0x00, // $05
		0x10, 0xF1, 0x0E, 0xF0, 0x00, 0x90, 0x00, 0x00, // $06
		0x50, 0xA1, 0x0E, 0xC0, 0x00, 0x80, 0x00, 0x00, // $07
		0x60, 0xB1, 0x0E, 0xB0, 0x00, 0x80, 0x00, 0x00, // $08
		0x60, 0x91, 0x0E, 0xA0, 0x00, 0x80, 0x00, 0x00, // $09
		0x90, 0x81, 0x0E, 0xB0, 0x00, 0x80, 0x00, 0x00, // $0A
		0xA0, 0x91, 0x0E, 0xB0, 0x00, 0x80, 0x00, 0x00, // $0B
		0xD0, 0x91, 0x0E, 0xB0, 0x00, 0x60, 0x00, 0x00, // $0C
		0xF0, 0x71, 0x0E, 0xB0, 0x00, 0x60, 0x00, 0x00, // $0D
		0xF0, 0x31, 0x0E, 0xB0, 0x00, 0x60, 0x00, 0x00, // $0E
		0xF0, 0x41, 0x0E, 0xB0, 0x00, 0x70, 0x00, 0x00, // $0F
		0xD0, 0x21, 0x0E, 0xA0, 0x00, 0x60, 0x00, 0x00, // $10
		0x70, 0x11, 0x0E, 0xB0, 0x00, 0x80, 0x00, 0x00, // $11
		0x50, 0x11, 0x0E, 0xB0, 0x00, 0x90, 0x00, 0x00, // $12
		0x80, 0x21, 0x0E, 0xA0, 0x00, 0x80, 0x00, 0x00, // $13
		0x30, 0x61, 0x0E, 0xA0, 0x00, 0xA0, 0x00, 0x00, // $14
		0x40, 0x41, 0x0E, 0xA0, 0x00, 0x90, 0x00, 0x00, // $15
		0x30, 0x11, 0x0E, 0x80, 0x00, 0xA0, 0x00, 0x00, // $16
		0x10, 0x01, 0x0E, 0x70, 0x00, 0xA0, 0x00, 0x00, // $17
		0x80, 0x41, 0x0E, 0xB0, 0x00, 0xA0, 0x00, 0x00, // $18
		0xA0, 0x31, 0x0E, 0xB0, 0x00, 0x80, 0x00, 0x00, // $19
		0xC0, 0x31, 0x0E, 0xB0, 0x00, 0x60, 0x00, 0x00, // $1A
		0xC0, 0x51, 0x0E, 0xB0, 0x00, 0x70, 0x00, 0x00, // $1B
		0x60, 0x41, 0x0E, 0x30, 0x00, 0x80, 0x00, 0x00, // $1C
		0x30, 0x11, 0x0E, 0x10, 0x00, 0x80, 0x00, 0x00, // $1D
		0x20, 0x31, 0x0E, 0x40, 0x00, 0x80, 0x00, 0x00, // $1E
		0x70, 0x61, 0x0E, 0x90, 0x00, 0x60, 0x00, 0x00, // $1F
		0x30, 0x31, 0x0E, 0xE0, 0x00, 0x70, 0x00, 0x00, // $20
		0x10, 0x51, 0x0E, 0xF0, 0x00, 0xF0, 0x00, 0x00, // $21
		0x50, 0x11, 0x0E, 0xE0, 0x00, 0x90, 0x00, 0x00, // $22
		0x30, 0x01, 0x0E, 0x90, 0x00, 0x80, 0x00, 0x00, // $23
		0x10, 0x31, 0x0C, 0xC0, 0x00, 0x80, 0x00, 0x00, // $24
		0x10, 0x91, 0x0C, 0xE0, 0x00, 0x80, 0x00, 0x00, // $25
		0x30, 0xA1, 0x0E, 0x80, 0x00, 0xA0, 0x00, 0x00, // $26
		0x41, 0x20, 0x0C, 0x80, 0x00, 0x00, 0xF0, 0x00, // $27
		0x41, 0x90, 0x04, 0xE0, 0x00, 0x00, 0xF0, 0x00, // $28
		0x31, 0xA0, 0x0C, 0x80, 0x00, 0x00, 0x40, 0x00, // $29
		0x70, 0x91, 0x0A, 0xC0, 0x00, 0x60, 0x00, 0x00, // $2A
		0x70, 0x91, 0x08, 0xC0, 0x00, 0xF0, 0x00, 0x00, // $2B
		0x71, 0x90, 0x0A, 0xC0, 0x00, 0x00, 0x80, 0x00, // $2C
		0x71, 0x90, 0x08, 0xC0, 0x00, 0x00, 0x80, 0x00, // $2D
		0x70, 0x91, 0x3A, 0xC0, 0x00, 0x40, 0x00, 0x00, // $2E
		0x30, 0x20, 0x06, 0xD0, 0x00, 0x20, 0xF0, 0x00, // $2F
		0x01, 0x70, 0x06, 0xC0, 0x00, 0x00, 0xF0, 0x00, // $30
		0x20, 0xB0, 0x0E, 0xE0, 0x00, 0x20, 0xF0, 0x00, // $31
		0x21, 0xB0, 0x0E, 0xE0, 0x00, 0x00, 0x90, 0x00, // $32
		0x20, 0x30, 0x0E, 0x90, 0x00, 0x60, 0x80, 0x00, // $33
		0x21, 0x30, 0x0E, 0x90, 0x00, 0x00, 0x80, 0x00, // $34
		0x30, 0x70, 0x06, 0xE0, 0x00, 0x20, 0x40, 0x00, // $35
		0x51, 0x80, 0x06, 0xA0, 0x00, 0x00, 0x60, 0x00, // $36
		0x00, 0x31, 0x3E, 0x90, 0x00, 0xF0, 0x00, 0x00, // $37
		0x00, 0x81, 0x3E, 0xD0, 0x00, 0xF0, 0x00, 0x00, // $38
		0x20, 0xC1, 0x3E, 0xE0, 0x00, 0x80, 0x00, 0x00, // $39
		0x70, 0x91, 0x0E, 0xA0, 0x00, 0x80, 0x00, 0x00, // $3A
		0x20, 0x81, 0x0E, 0x90, 0x00, 0x60, 0x00, 0x00, // $3B
		0x10, 0x71, 0x0E, 0x90, 0x00, 0xA0, 0x00, 0x00, // $3C
		0x00, 0x91, 0x0E, 0xA0, 0x00, 0xA0, 0x00, 0x00, // $3D
		0x60, 0x71, 0x0E, 0xA0, 0x00, 0x70, 0x00, 0x00, // $3E
		0x10, 0x11, 0x0E, 0xE0, 0x00, 0xF0, 0x00, 0x00, // $3F
	};
}

SSI263Synth::SSI263Synth(uint32_t clockHz)
	: m_clockHz(clockHz)
{
	SetClock(clockHz);
}

void SSI263Synth::SetClock(uint32_t clockHz)
{
	if (clockHz == 0 || clockHz > 4000000)
		throw std::invalid_argument("SSI263 clock is outside the supported range");
	m_clockHz = clockHz;
}

void SSI263Synth::Reset(uint32_t clockHz)
{
	*this = SSI263Synth(clockHz);
}

void SSI263Synth::SetFunction(uint8_t function)
{
	if (function > 3)
		throw std::invalid_argument("SSI263 function must be 0..3");
	m_function = function;
}

int SSI263Synth::LiveInflection() const
{
	return ((m_control.m_regs[2] & 8) << 8) | (m_control.m_regs[1] << 3) | (m_control.m_regs[2] & 7);
}

void SSI263Synth::StartPitch()
{
	int next = LiveInflection();
	if (m_function == 3)
	{
		if (m_pitchSeeded)
			next = (next & ~0x7C0) | (m_inflection & 0x7C0);
		m_pitchSeeded = true;
	}
	m_inflection = next;
}

void SSI263Synth::Write(uint8_t reg, uint8_t value)
{
	reg &= 7;
	if (reg >= 4)
		reg = 4;
	const bool oldControl = (m_control.m_regs[3] & 0x80) != 0;
	m_control.Write(reg, value);
	if (reg == 0 && !oldControl)
		StartPitch();
	else if (reg == 3 && oldControl && !(value & 0x80))
	{
		const int mode = m_control.m_regs[0] >> 6;
		if (mode != 0)
			m_function = mode;
		StartPitch();
	}
}

void SSI263Synth::Advance(uint32_t ticks)
{
	const int inflection = m_function == 3 ? m_inflection : LiveInflection();
	while (ticks--)
	{
		// The controller sees the envelope before this edge; source and tract follow.
		m_control.Tick(m_source.AmplitudeZero());
		m_source.Tick(m_control, inflection);
		m_tract.Process(m_source.m_output);
	}
}

void SSI263Synth::AdvancePitch()
{
	// Retain the listening model's 20 kHz glide. Its exact SSI timing is unknown.
	m_pitchCadence += 5;
	if (m_pitchCadence < 12)
		return;
	m_pitchCadence -= 12;
	int next = LiveInflection();
	if (m_function == 3)
	{
		static const int steps[8] = {1, 2, 3, 4, 6, 8, 12, 16};
		const int active = (m_inflection >> 6) & 31;
		const int target = (next >> 6) & 31;
		const int step = steps[(next >> 3) & 7];
		const int delta = target - active;
		const int moved = active + (delta < -step ? -step : delta > step ? step : delta);
		next = (next & ~0x7C0) | (moved << 6);
	}
	m_inflection = next;
}

int16_t SSI263Synth::GetSample()
{
	const int16_t sample = m_tract.GetSample();
	AdvancePitch();
	return sample;
}

void SSI263Synth::Ramp::Retarget(int value)
{
	m_target = value;
	m_step = (m_target - m_value + 16) & 15;
	m_fraction = 8;
	m_upward = m_target >= m_value;
}

void SSI263Synth::Ramp::Step()
{
	if (m_value == m_target)
		return;
	const int sum = m_step + m_fraction;
	m_fraction = sum & 15;
	const bool carry = sum >= 16;
	if (m_upward && carry)
		++m_value;
	if (!m_upward && !carry)
		--m_value;
}

SSI263Synth::Control::Control()
{
	RestartDuration();
}

int SSI263Synth::Control::ArticulationPeriod() const
{
	// Fixed reference RATE=8 keeps articulation independent of the speech RATE.
	return 256 * 8 * (8 - ((m_regs[3] >> 4) & 7));
}

int SSI263Synth::Control::DurationPeriod() const
{
	return 256 * (16 - (m_regs[2] >> 4)) * (4 - (m_regs[0] >> 6));
}

int SSI263Synth::Control::AmplitudePeriod() const
{
	return 128 * (16 - (m_regs[2] >> 4)) * ((m_regs[0] & 0x80) ? 1 : 2);
}

void SSI263Synth::Control::RestartDuration()
{
	m_durationPhase = 0;
	m_durationLeft = DurationPeriod();
	m_articulationLeft = ArticulationPeriod();
	m_amplitudeLeft = AmplitudePeriod();
	m_durationPending = m_durationWindow = false;
	m_articulationPending = m_articulationWindow = false;
	m_amplitudePending = m_amplitudeWindow = false;
}

void SSI263Synth::Control::Write(int reg, int value)
{
	const int old = m_regs[reg];
	m_regs[reg] = value;
	if (reg == 0)
	{
		m_phoneValid = true;
		m_pw0 = m_pw1 = 0;
		m_phoneSetupPending = true;
		// Restart one full duration interval; retain scanner and parameter state.
		m_durationPhase = 0;
		m_durationLeft = DurationPeriod();
		m_durationPending = m_durationWindow = false;
	}
	else if (reg == 3)
	{
		m_controlSetupPending = true;
		m_active = (value & 0x80) == 0;
		if ((old & 0x80) && m_active)
			RestartDuration();
	}
}

int SSI263Synth::Control::Target(int selector) const
{
	if (selector == 4)
		return m_regs[3] & 15;
	if ((selector == 5 || selector == 6) && (m_regs[3] & 15) == 0)
		return 0;
	return (phonemeRom[8 * (m_regs[0] & 63) + selector] >> 4) & 15;
}

bool SSI263Synth::Control::PermitTransition(int selector) const
{
	switch (selector)
	{
	case 0:
	case 1:
	case 3:
		return m_articulationWindow && And(m_pw5,
			((m_regs[0] & 32) || m_codes[5] || m_codes[6]) ? 1 : 0) == 0;
	case 2:
		return m_articulationWindow;
	case 4:
		// AMP=0 clears source targets but retains filter gain. U166B timing is approximate.
		return m_durationWindow && (m_regs[3] & 15) != 0;
	case 5:
		return m_amplitudeWindow && m_pw0 == 1;
	case 6:
		return m_amplitudeWindow && m_pw1 == 1;
	default:
		return false;
	}
}

void SSI263Synth::Control::WriteParameter()
{
	if (!m_phoneValid || m_selector == 7)
		return;
	const bool setup = (m_phoneSetupWindow && m_selector != 4) ||
		(m_controlSetupWindow && m_selector >= 4 && m_selector <= 6 &&
		(m_selector != 4 || (m_regs[3] & 15) != 0));
	if (setup)
		m_ramps[m_selector].Retarget(Target(m_selector));
	else if (!m_phoneSetupPending && m_active && PermitTransition(m_selector))
		m_ramps[m_selector].Step();
}

void SSI263Synth::Control::LatchParameter(bool rising, int amplitudeZero)
{
	if (!m_phoneValid)
		return;
	if (m_selector < 7)
		m_codes[m_selector] = m_ramps[m_selector].m_value;
	const int flags = phonemeRom[8 * (m_regs[0] & 63) + m_selector] & 15;
	if (m_selector < 2 && m_durationPhase == ((flags & 1) ? 2 : 6))
		(m_selector == 0 ? m_pw0 : m_pw1) = 1;
	else if (m_selector == 2)
	{
		// PW3 is transparent in phases 10 and 11; route/PW2/PW5 use only the edge.
		m_pw3 = LoadLatch(m_pw3, m_pw1, ((m_regs[3] & 0x80) || !(flags & 2)) ? 1 : 0);
		if (!rising)
			return;
		const int parameter2 = (flags >> 2) & 1;
		const int settled = Or(And(And(m_pw0, m_pw1), amplitudeZero), m_codes[6] == 0 ? 1 : 0);
		const int enable = And(And(m_pw1, Or(m_pw2, parameter2)), settled);
		m_route = LoadLatch(m_route, enable, (flags >> 3) & 1);
		m_pw2 = parameter2;
		m_pw5 = 1 - parameter2;
	}
}

void SSI263Synth::Control::Tick(int amplitudeZero)
{
	m_filterEdge = false;
	const int previousRoute = m_route;
	if (m_active && m_phoneValid)
	{
		if (--m_durationLeft == 0)
		{
			m_durationLeft = DurationPeriod();
			m_durationPhase = (m_durationPhase + 1) & 15;
			m_durationPending = true;
		}
		if (--m_articulationLeft == 0)
		{
			m_articulationLeft = ArticulationPeriod();
			m_articulationPending = true;
		}
		if (--m_amplitudeLeft == 0)
		{
			m_amplitudeLeft = AmplitudePeriod();
			m_amplitudePending = true;
		}
	}
	// The settled scan phase selects WRITE=2, LATCH=10, and next selector=0.
	m_scanPhase = (m_scanPhase + 1) & 15;
	if (m_scanPhase == 0)
	{
		const int oldSelector = m_selector;
		m_selector = (m_selector + 1) & 7;
		if (oldSelector == 3)
		{
			m_phoneSetupWindow = m_phoneSetupPending;
			m_controlSetupWindow = m_controlSetupPending;
			m_articulationWindow = m_articulationPending;
			m_amplitudeWindow = m_amplitudePending;
			m_durationWindow = m_durationPending;
			m_phoneSetupPending = m_controlSetupPending = false;
			m_articulationPending = m_amplitudePending = m_durationPending = false;
		}
	}
	if (m_scanPhase == 2)
		WriteParameter();
	if (m_scanPhase == 10 || m_scanPhase == 11)
		LatchParameter(m_scanPhase == 10, amplitudeZero);
	if (--m_filterLeft == 0)
	{
		m_filterLeft = 256 - m_regs[4];
		m_filterPhase = !m_filterPhase;
		m_filterEdge = true;
		// A coincident route write and Phi0 edge must latch the old route.
		if (!m_filterPhase)
			m_fric2 = previousRoute < 0 ? -1 : 1 - previousRoute;
	}
	if (m_filterPhase)
		m_fric1 = m_route;
}

SSI263Synth::Source::Source()
{
	// Deterministic cold route: FRIC1 avoids the spurious first-H hiss.
	// This seed and the voice/noise trims are not measured chip reset values.
	m_output.m_fric1 = true;
}

void SSI263Synth::Source::ShiftNoise()
{
	const int force = (m_noiseCount & 12) == 0 ? 1 : 0;
	const int feedback = force ^ ((m_noise1 >> 3) & 1) ^ ((m_noise2 >> 4) & 1) ^
		((m_noise4 >> 3) & 1) ^ ((m_noise4 >> 4) & 1);
	// All four CD4006 shift sections read their inputs before this edge.
	const int next1 = ((m_noise1 << 1) | ((m_noise3 >> 3) & 1)) & 15;
	const int next2 = ((m_noise2 << 1) | ((m_noise4 >> 4) & 1)) & 31;
	const int next3 = ((m_noise3 << 1) | ((m_noise2 >> 4) & 1)) & 15;
	const int next4 = ((m_noise4 << 1) | feedback) & 31;
	m_noise1 = next1;
	m_noise2 = next2;
	m_noise3 = next3;
	m_noise4 = next4;
}

bool SSI263Synth::Source::ResetVoice(const Control& control) const
{
	const bool closed = control.m_pw3 == 1 && !m_voiceToggle;
	const bool up = !closed && (m_output.m_codes.m_voiceAmp != 0 || m_output.m_codes.m_fricAmp != 0);
	return closed || (up ? m_amplitude != 15 : m_amplitude != 0);
}

void SSI263Synth::Source::Settle(const Control& control)
{
	// A gate can open while its selector is already high. Resolve those edges
	// between clock events; counting selector changes alone misses them.
	for (int pass = 0; pass < 16; ++pass)
	{
		bool changed = false;
		if (ResetVoice(control) && m_voiceToggle)
		{
			m_voiceToggle = false;
			changed = true;
		}
		const bool closed = control.m_pw3 == 1 && !m_voiceToggle;
		const bool up = !closed && (m_output.m_codes.m_voiceAmp != 0 || m_output.m_codes.m_fricAmp != 0);
		const bool notTerminal = up ? m_amplitude != 15 : m_amplitude != 0;
		const bool clock = (control.m_selector & 4) &&
			!((!notTerminal && up) || (!up && AmplitudeZero()));
		if (clock && !m_envelopeClock)
		{
			m_amplitude = (m_amplitude + (up ? 1 : 15)) & 15;
			changed = true;
		}
		if (clock != m_envelopeClock)
			changed = true;
		m_envelopeClock = clock;
		const bool noiseClock = !(control.m_pw3 == 1 && !m_voiceToggle) &&
			!(control.m_selector & 2) && m_output.m_codes.m_fricAmp != 0;
		if (noiseClock && !m_noiseClock)
			m_noiseCount = m_noiseCount == 15 ? 1 : m_noiseCount + 1;
		else if (!noiseClock && m_noiseClock)
			ShiftNoise();
		m_noiseClock = noiseClock;
		if (!changed)
			return;
	}
	assert(false && "SSI263 source gates failed to settle");
}

void SSI263Synth::Source::Tick(const Control& control, int inflection)
{
	const bool phase = control.m_filterPhase;
	const bool edge = control.m_filterEdge;
	const bool poweredDown = (control.m_regs[3] & 0x80) != 0;
	m_output.m_phase = phase;
	m_output.m_phaseEdge = edge;
	m_output.m_outputOpen = edge && !phase;
	Codes& codes = m_output.m_codes;
	// The phase latches remain open between edges.
	if (!phase)
	{
		codes.m_f1 = control.m_codes[0];
		codes.m_f3 = control.m_codes[3];
		codes.m_voiceAmp = control.m_codes[5];
	}
	else
	{
		codes.m_f2 = control.m_codes[1];
		codes.m_f2q = control.m_codes[2];
		codes.m_f4 = control.m_codes[3];
		codes.m_fricAmp = control.m_codes[6];
	}
	if (control.m_fric1 >= 0)
		m_output.m_fric1 = control.m_fric1 != 0;
	if (control.m_fric2 >= 0)
		m_output.m_fric2 = control.m_fric2 != 0;
	Settle(control);
	// Pitch writes take effect at divider reload; phone writes do not reset it.
	if (--m_voiceLeft == 0)
	{
		m_voiceLeft = 4 * (4096 - inflection);
		if (!ResetVoice(control))
			m_voiceToggle = !m_voiceToggle;
	}
	Settle(control);
	if (poweredDown)
		m_pitchSync = m_loadPending = false;
	else if (edge && !phase)
	{
		const bool oldSync = m_pitchSync;
		m_pitchSync = m_voiceToggle;
		if (m_voiceToggle && !oldSync)
			m_loadPending = true;
	}
	if (edge)
	{
		if (phase)
		{
			if (!poweredDown && m_loadPending)
			{
				m_voiceCount = 11; // Four Phi1 intervals before the counter reaches 15.
				m_loadPending = false;
			}
			else if (m_voiceCount != 15)
				++m_voiceCount;
		}
		else
		{
			const bool closed = control.m_pw3 == 1 && !m_voiceToggle;
			codes.m_filterAmp = control.m_codes[4] & ((m_amplitude & 14) | (closed ? 0 : 1));
		}
	}
	const bool closed = control.m_pw3 == 1 && !m_voiceToggle;
	const bool noise = !(((m_noise3 >> 3) & 1) || closed) && (!m_voiceToggle || codes.m_voiceAmp == 0);
	// CTL hard-mutes excitation but retains filter charge. This is a model policy.
	m_output.m_voiceDrive = !poweredDown && m_voiceCount != 15 ? -16384 : 0;
	m_output.m_fricDrive = poweredDown ? 0 : noise ? 301 : -301;
}

namespace
{
	const std::array<int, 4> voiceCaps{220, 430, 870, 1800};
	const std::array<int, 4> fric1Caps{270, 512, 1068, 2160};
	const std::array<int, 4> fric2Caps{270, 530, 1082, 2160};
	const std::array<int, 4> f1Caps{160, 330, 660, 1300};
	const std::array<int, 4> f2Caps{280, 560, 1120, 2300};
	const std::array<int, 4> f3Caps{210, 420, 820, 1640};
	const std::array<int, 4> f4Caps{200, 400, 820, 1620};
	const std::array<int, 4> ampCaps{76, 150, 300, 600};
	const std::array<int, 4> zeroCaps{0};
}

bool SSI263Synth::Codes::Equals(const Codes& other) const
{
	return m_f1 == other.m_f1 && m_f2 == other.m_f2 && m_f2q == other.m_f2q &&
		m_f3 == other.m_f3 && m_f4 == other.m_f4 && m_filterAmp == other.m_filterAmp &&
		m_voiceAmp == other.m_voiceAmp && m_fricAmp == other.m_fricAmp;
}

int SSI263Synth::Tract::Saturate(int64_t value)
{
	return static_cast<int>((value < -8388608 ? -8388608 : value > 8388607 ? 8388607 : value));
}

int SSI263Synth::Tract::Divide(int64_t numerator, int denominator)
{
	// Sum pF * Q16 plate charge before rounding once, with ties away from zero.
	const int64_t rounded = numerator < 0 ? numerator - denominator / 2 : numerator + denominator / 2;
	return Saturate(rounded / denominator);
}

int SSI263Synth::Tract::CapSum(int mask, const std::array<int, 4>& caps)
{
	int result = 0;
	for (int bit = 0; bit < 4; ++bit)
	{
		if (mask & (1 << bit))
			result += caps[bit];
	}
	return result;
}

int64_t SSI263Synth::Tract::Charge(int mask, int target, const std::array<int, 4>& plates,
	const std::array<int, 4>& caps)
{
	int64_t result = 0;
	for (int bit = 0; bit < 4; ++bit)
	{
		if (mask & (1 << bit))
			result += int64_t(caps[bit]) * (target - plates[bit]);
	}
	return result;
}

int64_t SSI263Synth::Tract::Weighted(int mask, const std::array<int, 4>& plates,
	const std::array<int, 4>& caps)
{
	int64_t result = 0;
	for (int bit = 0; bit < 4; ++bit)
	{
		if (mask & (1 << bit))
			result += int64_t(caps[bit]) * plates[bit];
	}
	return result;
}

void SSI263Synth::Tract::SetPlates(std::array<int, 4>& plates, int mask, int target)
{
	for (int bit = 0; bit < 4; ++bit)
	{
		if (mask & (1 << bit))
			plates[bit] = target;
	}
}

void SSI263Synth::Tract::MoveFormant(int index, int mask, int fixedCap,
	const std::array<int, 4>& caps, int denominator, int64_t extraCharge)
{
	Formant& formant = m_formants[index];
	const int delta = Divide(int64_t(fixedCap) * (formant.m_history - formant.m_fixedPlate) +
		Charge(mask, formant.m_history, formant.m_plates, caps) + extraCharge, denominator);
	formant.m_output = Saturate(int64_t(formant.m_output) - delta);
	formant.m_fixedPlate = formant.m_history;
	SetPlates(formant.m_plates, mask, formant.m_history);
}

void SSI263Synth::Tract::Process(const FilterEvent& event)
{
	// With zero initial charge and no input drive, every voltage stays zero.
	// Keep event history and all chip clocks running until the first excitation.
	if (!m_hasExcitation)
	{
		if (event.m_voiceDrive == 0 && event.m_fricDrive == 0)
		{
			m_applied = event;
			return;
		}
		m_hasExcitation = true;
	}

	const FilterEvent& e = event;
	const bool changed = e.m_phaseEdge || e.m_phase != m_applied.m_phase ||
		!e.m_codes.Equals(m_applied.m_codes) || e.m_voiceDrive != m_applied.m_voiceDrive ||
		e.m_fricDrive != m_applied.m_fricDrive || e.m_fric1 != m_applied.m_fric1 || e.m_fric2 != m_applied.m_fric2;
	if (!changed)
	{
		if (e.m_outputOpen)
			m_reconstruction = m_output;
		m_applied.m_outputOpen = e.m_outputOpen;
		return;
	}
	const bool phi0 = e.m_phaseEdge && !e.m_phase;
	const bool phi1 = e.m_phaseEdge && e.m_phase;
	const int oldF2q = m_applied.m_codes.m_f2q;
	const int oldFric = m_applied.m_fricDrive;
	m_applied = e;

	Formant& f1 = m_formants[0];
	Formant& f2 = m_formants[1];
	Formant& f3 = m_formants[2];
	Formant& f4 = m_formants[3];
	Formant& f5 = m_formants[4];
	// Continuously closed precharge/reset paths use the final mask. Open
	// capacitor plates retain their own voltage across code changes.
	if (!e.m_phase)
	{
		m_voice = 0;
		SetPlates(m_voicePlates, e.m_codes.m_voiceAmp, 0);
		f1.m_fixedPlate = f3.m_fixedPlate = f5.m_fixedPlate = 0;
		SetPlates(f1.m_plates, e.m_codes.m_f1, 0);
		SetPlates(f3.m_plates, e.m_codes.m_f3, 0);
		SetPlates(m_filterPlates, e.m_codes.m_filterAmp, f5.m_output);
	}
	else
	{
		m_fric1 = 0;
		SetPlates(m_fric1Plates, e.m_codes.m_fricAmp, 0);
		f2.m_fixedPlate = f4.m_fixedPlate = 0;
		SetPlates(m_f2qPlates, e.m_codes.m_f2q, 0);
		SetPlates(f2.m_plates, e.m_codes.m_f2, 0);
		SetPlates(f4.m_plates, e.m_codes.m_f4, 0);
		if (e.m_fric1)
			m_c143Plate = 0;
	}

	// U152/U154, including source edges while either phase stays open.
	if (phi0)
	{
		const int delta = Divide(int64_t(3600) * m_fric2Shape, 3900);
		m_fric2Source = Saturate(int64_t(m_fric2Source) + delta);
		m_fric2Shape = Saturate(int64_t(m_fric2Shape) + Divide(int64_t(-5700) * delta, 3900));
	}
	else if (phi1)
	{
		m_fric2Shape = Saturate(int64_t(m_fric2Shape) + Divide(int64_t(-3600) * m_fric2Source, 3900));
	}
	const int edgeDelta = Divide(-int64_t(CapSum(e.m_codes.m_fricAmp, fric2Caps)) * (e.m_fricDrive - oldFric), 3900);
	m_fric2Source = Saturate(int64_t(m_fric2Source) + edgeDelta);
	m_fric2Shape = Saturate(int64_t(m_fric2Shape) +
		Divide(int64_t(e.m_phase ? -9300 : -5700) * edgeDelta, 3900));
	m_c150Delta = e.m_phase ? edgeDelta : 0;
	m_c151Delta = e.m_phase && e.m_fric2 ? m_fric2Source - m_c151Plate : 0;
	if (e.m_fric2)
		m_c151Plate = m_fric2Source;

	const int oldVoice = m_voice;
	if (e.m_phase)
	{
		m_voice = Saturate(int64_t(m_voice) -
			Divide(Charge(e.m_codes.m_voiceAmp, e.m_voiceDrive, m_voicePlates, voiceCaps), 3300));
		SetPlates(m_voicePlates, e.m_codes.m_voiceAmp, e.m_voiceDrive);
		const int value = phi1 ? Divide(int64_t(11500) * f1.m_history +
			int64_t(2700) * (f1.m_output - m_voice) - int64_t(2700) * m_voice, 11700) :
			Divide(int64_t(-5400) * (m_voice - oldVoice), 11700);
		f1.m_history = phi1 ? value : Saturate(int64_t(f1.m_history) + value);
	}
	else
	{
		m_fric1 = Saturate(int64_t(m_fric1) -
			Divide(Charge(e.m_codes.m_fricAmp, e.m_fricDrive, m_fric1Plates, fric1Caps), 3900));
		SetPlates(m_fric1Plates, e.m_codes.m_fricAmp, e.m_fricDrive);
		const int newMask = (~oldF2q) & e.m_codes.m_f2q;
		if (phi0)
		{
			f2.m_history = Divide(int64_t(6800) * f2.m_history + int64_t(4700) * (f2.m_output - f1.m_output) +
				Weighted(e.m_codes.m_f2q, m_f2qPlates, voiceCaps), 7000 + CapSum(e.m_codes.m_f2q, voiceCaps));
		}
		else if (newMask)
		{
			const int keep = oldF2q & e.m_codes.m_f2q;
			const int denominator = 7000 + CapSum(keep, voiceCaps);
			f2.m_history = Divide(int64_t(denominator) * f2.m_history +
				Weighted(newMask, m_f2qPlates, voiceCaps), denominator + CapSum(newMask, voiceCaps));
		}
		if (phi0 || newMask)
			SetPlates(m_f2qPlates, e.m_codes.m_f2q, f2.m_history);
	}

	const int oldF1 = f1.m_output;
	if (e.m_phase)
	{
		MoveFormant(0, e.m_codes.m_f1, 250, f1Caps, 11500);
		const int value = phi1 ? Divide(int64_t(4700) * f3.m_history +
			int64_t(3900) * (f3.m_output - f2.m_output) + int64_t(2000) * (oldF1 - f1.m_output), 4900) :
			Divide(int64_t(-2000) * (f1.m_output - oldF1), 4900);
		f3.m_history = phi1 ? value : Saturate(int64_t(f3.m_history) + value);
		MoveFormant(2, e.m_codes.m_f3, 820, f3Caps, 4700);
		const int64_t sideCharge = int64_t(-1150) * m_c150Delta - int64_t(3700) * m_c151Delta;
		const int lastHistory = phi1 ? Divide(int64_t(3450) * f5.m_history +
			int64_t(4700) * (f5.m_output - f4.m_output) + sideCharge, 3730) : Divide(sideCharge, 3730);
		f5.m_history = phi1 ? lastHistory : Saturate(int64_t(f5.m_history) + lastHistory);
		MoveFormant(4, 0, 4700, zeroCaps, 3450);
		const int64_t outputCharge = Charge(e.m_codes.m_filterAmp, f5.m_output, m_filterPlates, ampCaps);
		if (phi1)
			m_output = Divide(int64_t(2700) * m_output - outputCharge, 2750);
		else
			m_output = Saturate(int64_t(m_output) - Divide(outputCharge, 2750));
		SetPlates(m_filterPlates, e.m_codes.m_filterAmp, f5.m_output);
	}
	else
	{
		const int c143Delta = e.m_fric1 ? m_c143Plate - m_fric1 : 0;
		MoveFormant(1, e.m_codes.m_f2, 500, f2Caps, 6800, int64_t(-1000) * c143Delta);
		if (e.m_fric1)
			m_c143Plate = m_fric1;
		if (phi0)
			f4.m_history = Divide(int64_t(4300) * f4.m_history +
				int64_t(4700) * (f4.m_output - f3.m_output), 4500);
		MoveFormant(3, e.m_codes.m_f4, 1670, f4Caps, 4300);
	}
	if (e.m_outputOpen)
		m_reconstruction = m_output;
}

int16_t SSI263Synth::Tract::GetSample() const
{
	// Floor division preserves the signed Q16-to-PCM rounding, including -1 -> -1.
	const int output = m_reconstruction >= 0 ? m_reconstruction / 2 : -((-m_reconstruction + 1) / 2);
	return static_cast<int16_t>(output < -32768 ? -32768 : output > 32767 ? 32767 : output);
}

namespace
{
	struct StateWriter
	{
		std::vector<uint32_t> m_words;
		void Value(int& value, int, int) { m_words.push_back(static_cast<uint32_t>(value)); }
		void Value(bool& value) { m_words.push_back(value ? 1 : 0); }
	};

	struct StateReader
	{
		explicit StateReader(const std::vector<uint32_t>& words) : m_words(words) {}
		const std::vector<uint32_t>& m_words;
		std::size_t m_next = 0;
		bool m_valid = true;

		void Value(int& value, int minimum, int maximum)
		{
			if (m_next == m_words.size())
			{
				m_valid = false;
				return;
			}
			const uint32_t word = m_words[m_next++];
			const int64_t decoded = word <= INT32_MAX ? int64_t(word) : int64_t(word) - 0x100000000LL;
			if (decoded < minimum || decoded > maximum)
				m_valid = false;
			else
				value = static_cast<int>(decoded);
		}

		void Value(bool& value)
		{
			int bit = 0;
			Value(bit, 0, 1);
			value = bit != 0;
		}
	};
}

template<class State> void SSI263Synth::VisitState(State& state)
{
	// Keep this order fixed within a version. Never save raw structs or padding.
	int version = 1;
	state.Value(version, 1, 1);
	int clockHz = static_cast<int>(m_clockHz);
	state.Value(clockHz, 1, 4000000);
	m_clockHz = static_cast<uint32_t>(clockHz);
	state.Value(m_function, 0, 3);
	state.Value(m_inflection, 0, 4095);
	state.Value(m_pitchCadence, 0, 11);
	state.Value(m_pitchSeeded);
	for (int& reg : m_control.m_regs)
		state.Value(reg, 0, 255);
	for (Ramp& ramp : m_control.m_ramps)
	{
		state.Value(ramp.m_value, 0, 15);
		state.Value(ramp.m_step, 0, 15);
		state.Value(ramp.m_fraction, 0, 15);
		state.Value(ramp.m_target, 0, 15);
		state.Value(ramp.m_upward);
	}
	for (int& code : m_control.m_codes)
		state.Value(code, 0, 15);
	state.Value(m_control.m_selector, 0, 7);
	state.Value(m_control.m_scanPhase, 0, 15);
	state.Value(m_control.m_durationPhase, 0, 15);
	state.Value(m_control.m_durationLeft, 1, 16384);
	state.Value(m_control.m_articulationLeft, 1, 16384);
	state.Value(m_control.m_amplitudeLeft, 1, 4096);
	state.Value(m_control.m_active);
	state.Value(m_control.m_phoneValid);
	state.Value(m_control.m_phoneSetupPending);
	state.Value(m_control.m_phoneSetupWindow);
	state.Value(m_control.m_controlSetupPending);
	state.Value(m_control.m_controlSetupWindow);
	state.Value(m_control.m_articulationPending);
	state.Value(m_control.m_articulationWindow);
	state.Value(m_control.m_amplitudePending);
	state.Value(m_control.m_amplitudeWindow);
	state.Value(m_control.m_durationPending);
	state.Value(m_control.m_durationWindow);
	for (int* value : {&m_control.m_pw0, &m_control.m_pw1, &m_control.m_pw2, &m_control.m_pw3,
		&m_control.m_pw5, &m_control.m_route, &m_control.m_fric1, &m_control.m_fric2})
		state.Value(*value, -1, 1);
	state.Value(m_control.m_filterLeft, 1, 256);
	state.Value(m_control.m_filterPhase);
	state.Value(m_control.m_filterEdge);
	state.Value(m_source.m_noise1, 0, 15);
	state.Value(m_source.m_noise2, 0, 31);
	state.Value(m_source.m_noise3, 0, 15);
	state.Value(m_source.m_noise4, 0, 31);
	state.Value(m_source.m_noiseCount, 1, 15);
	state.Value(m_source.m_voiceLeft, 1, 16384);
	state.Value(m_source.m_voiceCount, 0, 15);
	state.Value(m_source.m_amplitude, 0, 15);
	state.Value(m_source.m_voiceToggle);
	state.Value(m_source.m_envelopeClock);
	state.Value(m_source.m_noiseClock);
	state.Value(m_source.m_pitchSync);
	state.Value(m_source.m_loadPending);
	for (FilterEvent* event : {&m_source.m_output, &m_tract.m_applied})
	{
		state.Value(event->m_phase);
		state.Value(event->m_phaseEdge);
		Codes& codes = event->m_codes;
		for (int* code : {&codes.m_f1, &codes.m_f2, &codes.m_f2q, &codes.m_f3, &codes.m_f4,
			&codes.m_filterAmp, &codes.m_voiceAmp, &codes.m_fricAmp})
			state.Value(*code, 0, 15);
		state.Value(event->m_voiceDrive, -8388608, 8388607);
		state.Value(event->m_fricDrive, -131072, 131071);
		state.Value(event->m_fric1);
		state.Value(event->m_fric2);
		state.Value(event->m_outputOpen);
	}
	for (int* value : {&m_tract.m_voice, &m_tract.m_fric1, &m_tract.m_fric2Source, &m_tract.m_fric2Shape,
		&m_tract.m_c143Plate, &m_tract.m_c151Plate, &m_tract.m_c150Delta,
		&m_tract.m_output, &m_tract.m_reconstruction})
		state.Value(*value, -8388608, 8388607);
	state.Value(m_tract.m_c151Delta, -16777215, 16777215);
	for (std::array<int, 4>* plates : {&m_tract.m_voicePlates, &m_tract.m_fric1Plates,
		&m_tract.m_f2qPlates, &m_tract.m_filterPlates})
		for (int& plate : *plates)
			state.Value(plate, -8388608, 8388607);
	for (Formant& formant : m_tract.m_formants)
	{
		state.Value(formant.m_output, -8388608, 8388607);
		state.Value(formant.m_history, -8388608, 8388607);
		state.Value(formant.m_fixedPlate, -8388608, 8388607);
		for (int& plate : formant.m_plates)
			state.Value(plate, -8388608, 8388607);
	}
	state.Value(m_tract.m_hasExcitation);
}

std::vector<uint32_t> SSI263Synth::SaveState() const
{
	SSI263Synth copy = *this;
	StateWriter writer;
	copy.VisitState(writer);
	return writer.m_words;
}

bool SSI263Synth::LoadState(const std::vector<uint32_t>& words)
{
	SSI263Synth copy = *this;
	StateReader reader(words);
	copy.VisitState(reader);
	if (!reader.m_valid || reader.m_next != words.size())
		return false;
	for (const Ramp& ramp : copy.m_control.m_ramps)
	{
		if ((ramp.m_upward && ramp.m_target < ramp.m_value) ||
			(!ramp.m_upward && ramp.m_target > ramp.m_value))
			return false;
	}
	if (!copy.m_tract.m_hasExcitation)
	{
		// A loaded cold-state flag must satisfy the fast path's zero-charge invariant.
		const Tract& tract = copy.m_tract;
		for (int value : {tract.m_voice, tract.m_fric1, tract.m_fric2Source, tract.m_fric2Shape,
			tract.m_c143Plate, tract.m_c151Plate, tract.m_c150Delta, tract.m_c151Delta,
			tract.m_output, tract.m_reconstruction, tract.m_applied.m_voiceDrive, tract.m_applied.m_fricDrive})
			if (value != 0)
				return false;
		for (const std::array<int, 4>* plates : {&tract.m_voicePlates, &tract.m_fric1Plates,
			&tract.m_f2qPlates, &tract.m_filterPlates})
			for (int plate : *plates)
				if (plate != 0)
					return false;
		for (const Formant& formant : tract.m_formants)
		{
			if (formant.m_output != 0 || formant.m_history != 0 || formant.m_fixedPlate != 0)
				return false;
			for (int plate : formant.m_plates)
				if (plate != 0)
					return false;
		}
	}
	*this = copy;
	return true;
}
