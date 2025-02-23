#pragma once

#include <cstdlib>
#include <memory>
#include <vector>

class SoundBuffer;

namespace ra2
{
    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName);

    void writeAudio(const size_t fps, const size_t sampleRate, const size_t channels, std::vector<int16_t> &buffer);
    void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
} // namespace ra2
