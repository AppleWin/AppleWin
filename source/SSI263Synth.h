#pragma once

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

Portable SSI263 sound state and interface.
Adapted from the Appletini ONE project at https://github.com/hasseily/appletini-one
*/


#include <array>
#include <cstdint>
#include <vector>

// SSI-263 speech from its parameter ROM and an ideal SC-02 prototype circuit.
// Pitch glide, cold state and source levels remain model approximations.
// The card owns bus timing and interrupts; this class only produces audio.
class SSI263Synth
{
public:
	static const uint32_t kSampleRate = 48000;

	explicit SSI263Synth(uint32_t clockHz = 1015625);
	void Reset(uint32_t clockHz);
	void SetClock(uint32_t clockHz);
	// Restore a saved mode, or relatch it on SSI263P reset, without a CTL edge.
	void SetFunction(uint8_t function);
	void Write(uint8_t reg, uint8_t value);
	void Advance(uint32_t ticks);

	// Call once per 48 kHz output sample, after advancing to its clock time.
	int16_t GetSample();
	uint32_t GetClockHz() const { return m_clockHz; }

	// Versioned words, independent of struct layout. A failed load changes nothing.
	std::vector<uint32_t> SaveState() const;
	bool LoadState(const std::vector<uint32_t>& words);

private:
	struct Ramp
	{
		int m_value = 0;
		int m_step = 0;
		int m_fraction = 8;
		int m_target = 0;
		bool m_upward = true;
		void Retarget(int value);
		void Step();
	};

	struct Control
	{
		Control();
		void Write(int reg, int value);
		void Tick(int amplitudeZero);
		void RestartDuration();
		void WriteParameter();
		void LatchParameter(bool rising, int amplitudeZero);
		int Target(int selector) const;
		bool PermitTransition(int selector) const;
		int ArticulationPeriod() const;
		int DurationPeriod() const;
		int AmplitudePeriod() const;

		std::array<int, 5> m_regs = {{0xC0, 0, 0, 0x80, 0xFF}};
		std::array<Ramp, 8> m_ramps;
		// F1, F2, F2Q, shared F3/F4, filter gain, voice, noise, unused.
		std::array<int, 8> m_codes = {{0}};
		int m_selector = 0;
		int m_scanPhase = 0;
		int m_durationPhase = 0;
		int m_durationLeft = 0;
		int m_articulationLeft = 0;
		int m_amplitudeLeft = 0;
		bool m_active = false;
		bool m_phoneValid = false;
		bool m_phoneSetupPending = false;
		bool m_phoneSetupWindow = false;
		bool m_controlSetupPending = false;
		bool m_controlSetupWindow = false;
		bool m_articulationPending = false;
		bool m_articulationWindow = false;
		bool m_amplitudePending = false;
		bool m_amplitudeWindow = false;
		bool m_durationPending = false;
		bool m_durationWindow = false;
		// -1 denotes a latch whose cold state is unknown.
		int m_pw0 = -1;
		int m_pw1 = -1;
		int m_pw2 = -1;
		int m_pw3 = -1;
		int m_pw5 = -1;
		int m_route = -1;
		int m_fric1 = -1;
		int m_fric2 = -1;
		int m_filterLeft = 1;
		bool m_filterPhase = false;
		bool m_filterEdge = false;
	};

	struct Codes
	{
		int m_f1 = 0;
		int m_f2 = 0;
		int m_f2q = 0;
		int m_f3 = 0;
		int m_f4 = 0;
		int m_filterAmp = 0;
		int m_voiceAmp = 0;
		int m_fricAmp = 0;
		bool Equals(const Codes& other) const;
	};

	struct FilterEvent
	{
		bool m_phase = false; // false = Phi0; true = Phi1.
		bool m_phaseEdge = false;
		Codes m_codes;
		int m_voiceDrive = 0;
		int m_fricDrive = 0;
		bool m_fric1 = false;
		bool m_fric2 = false;
		bool m_outputOpen = true;
	};

	struct Source
	{
		Source();
		void Tick(const Control& control, int inflection);
		void Settle(const Control& control);
		bool ResetVoice(const Control& control) const;
		int AmplitudeZero() const { return (m_amplitude & 14) == 0 ? 1 : 0; }
		void ShiftNoise();

		FilterEvent m_output;
		int m_noise1 = 1;
		int m_noise2 = 0;
		int m_noise3 = 0;
		int m_noise4 = 0;
		int m_noiseCount = 15;
		int m_voiceLeft = 16384;
		int m_voiceCount = 15;
		int m_amplitude = 0;
		bool m_voiceToggle = false;
		bool m_envelopeClock = false;
		bool m_noiseClock = false;
		bool m_pitchSync = false;
		bool m_loadPending = false;
	};

	struct Formant
	{
		int m_output = 0;
		int m_history = 0;
		int m_fixedPlate = 0;
		std::array<int, 4> m_plates = {{0}};
	};

	struct Tract
	{
		// Voltages use signed 24-bit Q16; capacitor values are in pF.
		void Process(const FilterEvent& event);
		int16_t GetSample() const;
		static int Saturate(int64_t value);
		static int Divide(int64_t numerator, int denominator);
		static int CapSum(int mask, const std::array<int, 4>& caps);
		static int64_t Charge(int mask, int target, const std::array<int, 4>& plates,
			const std::array<int, 4>& caps);
		static int64_t Weighted(int mask, const std::array<int, 4>& plates,
			const std::array<int, 4>& caps);
		static void SetPlates(std::array<int, 4>& plates, int mask, int target);
		void MoveFormant(int index, int mask, int fixedCap, const std::array<int, 4>& caps,
			int denominator, int64_t extraCharge = 0);

		int m_voice = 0;
		int m_fric1 = 0;
		int m_fric2Source = 0;
		int m_fric2Shape = 0;
		std::array<int, 4> m_voicePlates = {{0}};
		std::array<int, 4> m_fric1Plates = {{0}};
		std::array<int, 4> m_f2qPlates = {{0}};
		std::array<int, 4> m_filterPlates = {{0}};
		std::array<Formant, 5> m_formants;
		int m_c143Plate = 0;
		int m_c151Plate = 0;
		int m_c150Delta = 0;
		int m_c151Delta = 0;
		int m_output = 0;
		int m_reconstruction = 0;
		FilterEvent m_applied;
		bool m_hasExcitation = false;
	};

	int LiveInflection() const;
	void StartPitch();
	void AdvancePitch();
	template<class State> void VisitState(State& state);

	uint32_t m_clockHz;
	Control m_control;
	Source m_source;
	Tract m_tract;
	int m_function = 0;
	int m_inflection = 0;
	int m_pitchCadence = 0;
	bool m_pitchSeeded = false;
};
