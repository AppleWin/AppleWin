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

Render timed SSI263 register writes to stereo PCM.
*/

#include "../../source/SSI263Synth.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct RegisterEvent
{
	int64_t tick;
	uint32_t socket;
	uint32_t reg;
	uint32_t value;
};

static int64_t FloorDivide(int64_t numerator, int64_t denominator)
{
	return numerator / denominator - (numerator % denominator < 0);
}

static int64_t CeilDivide(int64_t numerator, int64_t denominator)
{
	return -FloorDivide(-numerator, denominator);
}

static void WriteSample(std::ostream& output, int16_t sample)
{
	const uint16_t value = static_cast<uint16_t>(sample);
	output.put(static_cast<char>(value & 255));
	output.put(static_cast<char>(value >> 8));
}

static void RenderTrace(const char* inputPath, const char* outputPath)
{
	std::ifstream input(inputPath);
	std::string magic;
	uint32_t clockHz;
	int64_t startTick, endTick;
	size_t eventCount;
	input >> magic >> clockHz >> startTick >> endTick >> eventCount;
	if (!input || magic != "SSIHOST1" || clockHz < 100000 || clockHz > 4000000 ||
		startTick < 0 || endTick <= startTick || endTick > int64_t(clockHz) * 3600 || eventCount > 10000000)
		throw std::runtime_error("invalid trace header");
	std::vector<RegisterEvent> events(eventCount);
	for (size_t index = 0; index < events.size(); index++)
	{
		RegisterEvent& event = events[index];
		input >> event.tick >> event.socket >> event.reg >> event.value;
		if (!input || event.tick < -int64_t(clockHz) || event.tick >= endTick ||
			event.socket > 1 || event.reg > 7 || event.value > 255 || (index && event.tick < events[index - 1].tick))
			throw std::runtime_error("invalid or unsorted register event");
	}
	std::string trailing;
	if (input >> trailing)
		throw std::runtime_error("unexpected trailing trace data");

	SSI263Synth chips[2] = {SSI263Synth(clockHz), SSI263Synth(clockHz)};
	const int64_t firstTick = events.empty() ? 0 : std::min<int64_t>(0, events.front().tick);
	const int64_t firstFrame = FloorDivide(firstTick * SSI263Synth::kSampleRate, clockHz);
	const int64_t startFrame = CeilDivide(startTick * SSI263Synth::kSampleRate, clockHz);
	const int64_t endFrame = CeilDivide(endTick * SSI263Synth::kSampleRate, clockHz);
	int64_t currentTick = CeilDivide(firstFrame * clockHz, SSI263Synth::kSampleRate);
	size_t nextEvent = 0;
	std::ofstream output(outputPath, std::ios::binary);
	if (!output)
		throw std::runtime_error("cannot open PCM output");
	const auto Advance = [&](int64_t tick)
	{
		int64_t remaining = tick - currentTick;
		while (remaining > 0)
		{
			const uint32_t ticks = static_cast<uint32_t>(std::min<int64_t>(remaining, 0xffffffff));
			for (SSI263Synth& chip : chips)
				chip.Advance(ticks);
			remaining -= ticks;
		}
		currentTick = tick;
	};
	for (int64_t frame = firstFrame; frame < endFrame; frame++)
	{
		const int64_t tick = CeilDivide(frame * clockHz, SSI263Synth::kSampleRate);
		while (nextEvent < events.size() && events[nextEvent].tick <= tick)
		{
			const RegisterEvent& event = events[nextEvent++];
			Advance(event.tick);
			chips[event.socket].Write(static_cast<uint8_t>(event.reg), static_cast<uint8_t>(event.value));
		}
		Advance(tick);
		for (SSI263Synth& chip : chips)
		{
			const int16_t sample = chip.GetSample();
			if (frame >= startFrame)
				WriteSample(output, sample);
		}
	}
	output.close();
	if (!output)
		throw std::runtime_error("PCM write failed");
	std::cout << "{\"frames\":" << endFrame - startFrame << "}\n";
}

int main(int argc, char** argv)
{
	try
	{
		if (argc != 3)
			throw std::runtime_error("usage: RenderTrace EVENTS OUTPUT.pcm");
		RenderTrace(argv[1], argv[2]);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
