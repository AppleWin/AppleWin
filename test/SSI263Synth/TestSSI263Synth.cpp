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

SSI263 synthesis and state regression tests.
*/

#include "../../source/SSI263Synth.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static const uint32_t kClockHz = 1015625;

static void Check(bool condition, const char* message)
{
	if (!condition)
		throw std::runtime_error(message);
}

static void StartVoice(SSI263Synth& synth, uint8_t function = 2)
{
	synth.Write(3, 0x80);
	synth.Write(0, static_cast<uint8_t>(function << 6));
	synth.Write(1, 0x6f);
	synth.Write(2, 0x8e);
	synth.Write(4, 128);
	synth.Write(3, 0x5c);
	synth.Write(0, 0x0e);
}

static std::vector<int16_t> Render(SSI263Synth& synth, uint32_t frames)
{
	std::vector<int16_t> samples;
	uint64_t previousTick = 0;
	for (uint32_t frame = 0; frame < frames; frame++)
	{
		const uint64_t tick = (uint64_t(frame) * synth.GetClockHz() + SSI263Synth::kSampleRate - 1) / SSI263Synth::kSampleRate;
		synth.Advance(static_cast<uint32_t>(tick - previousTick));
		previousTick = tick;
		samples.push_back(synth.GetSample());
	}
	return samples;
}

static void TestReset()
{
	SSI263Synth synth(kClockHz);
	const std::vector<uint32_t> coldState = synth.SaveState();
	StartVoice(synth);
	const std::vector<int16_t> expected = Render(synth, 4800);
	Check(std::any_of(expected.begin(), expected.end(), [](int16_t value) { return value != 0; }), "voice stayed silent");
	// FNV-1a over little-endian PCM from the accepted native model, 7 October 2026.
	uint64_t hash = 14695981039346656037ULL;
	for (int16_t sample : expected)
	{
		const uint16_t value = static_cast<uint16_t>(sample);
		hash = (hash ^ (value & 255)) * 1099511628211ULL;
		hash = (hash ^ (value >> 8)) * 1099511628211ULL;
	}
	Check(hash == 0xa2a114dcd3b1d24cULL, "voice PCM differs from the reference");
	synth.Reset(kClockHz);
	Check(synth.SaveState() == coldState, "reset did not restore cold state");
	StartVoice(synth);
	Check(Render(synth, 4800) == expected, "reset changed the PCM sequence");
	synth.Reset(1022727);
	Check(synth.GetClockHz() == 1022727, "reset did not select the new clock");
}

static void TestRegisterAliases()
{
	for (uint8_t reg = 0; reg < 8; reg++)
	{
		SSI263Synth reference(kClockHz);
		SSI263Synth alias(kClockHz);
		StartVoice(reference);
		StartVoice(alias);
		const uint8_t decoded = reg < 4 ? reg : 4;
		for (uint32_t value = 0; value < 256; value++)
		{
			reference.Write(decoded, static_cast<uint8_t>(value));
			alias.Write(reg, static_cast<uint8_t>(value));
			reference.Advance(37);
			alias.Advance(37);
			Check(reference.GetSample() == alias.GetSample(), "register alias changed PCM");
			Check(reference.SaveState() == alias.SaveState(), "register alias changed state");
		}
	}
}

static void TestAdvanceBatching()
{
	SSI263Synth batched(kClockHz);
	SSI263Synth singleTick(kClockHz);
	StartVoice(batched, 3);
	StartVoice(singleTick, 3);
	for (uint32_t frame = 0; frame < 4800; frame++)
	{
		if (frame % 137 == 0)
		{
			batched.Write(1, static_cast<uint8_t>(frame));
			singleTick.Write(1, static_cast<uint8_t>(frame));
		}
		const uint32_t ticks = 1 + frame % 43;
		batched.Advance(ticks);
		for (uint32_t tick = 0; tick < ticks; tick++)
			singleTick.Advance(1);
		Check(batched.GetSample() == singleTick.GetSample(), "Advance batching changed PCM");
	}
	Check(batched.SaveState() == singleTick.SaveState(), "Advance batching changed state");
	const std::vector<uint32_t> before = batched.SaveState();
	batched.Advance(0);
	Check(batched.SaveState() == before, "zero ticks changed state");
}

static void TestChipIndependence()
{
	SSI263Synth reference(kClockHz);
	SSI263Synth first(kClockHz);
	SSI263Synth second(1022727);
	StartVoice(reference);
	StartVoice(first);
	StartVoice(second, 3);
	for (uint32_t frame = 0; frame < 4800; frame++)
	{
		reference.Advance(21);
		first.Advance(21);
		second.Write(static_cast<uint8_t>(frame & 7), static_cast<uint8_t>(frame * 17));
		second.Advance(53);
		second.GetSample();
		Check(first.GetSample() == reference.GetSample(), "another chip changed PCM");
	}
	Check(first.SaveState() == reference.SaveState(), "another chip changed state");
}

static void TestSnapshot()
{
	SSI263Synth original(kClockHz);
	StartVoice(original, 3);
	Render(original, 4799);
	original.Write(1, 0xe5);
	original.Write(0, 0x2c);
	original.Advance(7);
	const std::vector<uint32_t> snapshot = original.SaveState();
	SSI263Synth restored(1022727);
	Check(restored.LoadState(snapshot), "valid snapshot was rejected");
	Check(restored.SaveState() == snapshot, "snapshot did not restore all state");
	Check(Render(original, 9600) == Render(restored, 9600), "snapshot continuation changed PCM");
	Check(original.SaveState() == restored.SaveState(), "snapshot continuation changed state");

	const std::vector<uint32_t> before = restored.SaveState();
	std::vector<uint32_t> invalid = snapshot;
	invalid[0] = 0xffffffff;
	Check(!restored.LoadState(invalid), "unknown snapshot version was accepted");
	Check(restored.SaveState() == before, "bad snapshot version changed state");
	invalid = snapshot;
	invalid.pop_back();
	Check(!restored.LoadState(invalid), "truncated snapshot was accepted");
	Check(restored.SaveState() == before, "truncated snapshot changed state");
	invalid = snapshot;
	invalid.push_back(0);
	Check(!restored.LoadState(invalid), "oversized snapshot was accepted");
	Check(restored.SaveState() == before, "oversized snapshot changed state");
	invalid.clear();
	Check(!restored.LoadState(invalid), "empty snapshot was accepted");
	Check(restored.SaveState() == before, "empty snapshot changed state");
	invalid = snapshot;
	Check(invalid.back() == 1, "speech did not record prior filter excitation");
	invalid.back() = 0;
	Check(!restored.LoadState(invalid), "charged filters accepted an unexcited snapshot flag");
	Check(restored.SaveState() == before, "invalid filter excitation flag changed state");
	for (size_t index = 0; index < snapshot.size(); index++)
	{
		invalid = snapshot;
		invalid[index] = 0x80000000;
		Check(!restored.LoadState(invalid), "out-of-range snapshot field was accepted");
		Check(restored.SaveState() == before, "out-of-range snapshot field changed state");
	}

	restored.SetClock(1022727);
	Check(restored.GetClockHz() == 1022727, "clock change was ignored");
	restored.SetClock(kClockHz);
	Check(restored.SaveState() == before, "clock change reset running synthesis state");
}

static void TestRegisterExtremes()
{
	const uint8_t limits[] = {0, 1, 127, 128, 254, 255};
	for (uint8_t limit : limits)
	{
		SSI263Synth synth(kClockHz);
		StartVoice(synth);
		synth.Write(1, limit);
		synth.Write(2, limit);
		synth.Write(4, limit);
		for (uint8_t phone = 0; phone < 64; phone++)
		{
			synth.Write(0, phone);
			synth.Write(3, static_cast<uint8_t>(limit & 0x7f));
			const std::vector<uint32_t> before = synth.SaveState();
			const std::vector<int16_t> expected = Render(synth, 240);
			const std::vector<uint32_t> after = synth.SaveState();
			Check(synth.LoadState(before), "extreme register state was rejected");
			Check(Render(synth, 240) == expected, "extreme register continuation changed PCM");
			Check(synth.SaveState() == after, "extreme register continuation changed state");
		}
	}
}

static void TestFunctionRestore()
{
	SSI263Synth synth(kClockHz);
	StartVoice(synth, 3);
	Render(synth, 4800);
	const std::vector<uint32_t> before = synth.SaveState();
	for (uint8_t function = 0; function < 4; function++)
	{
		synth.SetFunction(function);
		std::vector<uint32_t> expected = before;
		// Version 1 stores the latched function after the version and clock.
		expected[2] = function;
		Check(synth.SaveState() == expected, "function restore changed running synthesis state");
	}
	const uint8_t invalidFunctions[] = {4, 255};
	for (uint8_t function : invalidFunctions)
	{
		bool rejected = false;
		try
		{
			synth.SetFunction(function);
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		Check(rejected, "invalid function was accepted");
		Check(synth.SaveState() == before, "invalid function changed state");
	}

	SSI263Synth immediate(kClockHz);
	SSI263Synth transitioned(kClockHz);
	Check(immediate.LoadState(before) && transitioned.LoadState(before), "function test snapshot was rejected");
	immediate.SetFunction(2);
	transitioned.SetFunction(3);
	bool differentPitch = false;
	for (uint32_t frame = 0; frame < 4800; frame++)
	{
		// Keep moving the target so the next divider reload sees an active glide.
		if (frame % 11 == 0)
		{
			const uint8_t inflection = (frame / 11) & 1 ? 255 : 0;
			immediate.Write(1, inflection);
			transitioned.Write(1, inflection);
		}
		immediate.Advance(21);
		transitioned.Advance(21);
		if (immediate.GetSample() != transitioned.GetSample())
			differentPitch = true;
	}
	Check(differentPitch, "restored function did not select pitch glide");
}

static void TestColdSnapshot()
{
	SSI263Synth cold(kClockHz);
	const std::vector<int16_t> silence = Render(cold, SSI263Synth::kSampleRate);
	Check(std::all_of(silence.begin(), silence.end(), [](int16_t value) { return value == 0; }), "cold chip produced sound");
	Check(cold.SaveState().back() == 0, "cold chip recorded filter excitation");
	SSI263Synth restored(kClockHz);
	Check(restored.LoadState(cold.SaveState()), "cold snapshot was rejected");
	StartVoice(cold);
	StartVoice(restored);
	const std::vector<int16_t> expected = Render(cold, 4800);
	Check(std::any_of(expected.begin(), expected.end(), [](int16_t value) { return value != 0; }), "cold chip did not start speech");
	Check(Render(restored, 4800) == expected, "cold snapshot changed the first speech samples");
	Check(restored.SaveState() == cold.SaveState(), "cold snapshot changed the first speech state");
}

int main()
{
	try
	{
		TestReset();
		TestRegisterAliases();
		TestAdvanceBatching();
		TestChipIndependence();
		TestSnapshot();
		TestRegisterExtremes();
		TestFunctionRestore();
		TestColdSnapshot();
		std::cout << "SSI263Synth: all tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "SSI263Synth: " << error.what() << '\n';
		return 1;
	}
}
