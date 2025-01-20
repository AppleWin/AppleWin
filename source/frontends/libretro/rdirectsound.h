#pragma once

#include <cstdlib>
#include <memory>

#define REGVALUE_AUDIO_SPEAKER "Speaker"
#define REGVALUE_AUDIO_MOCKINGBOARD "Mockingboard"

class SoundBuffer;

namespace ra2
{
    enum class AudioSource
    {
        SPEAKER = 0,
        MOCKINGBOARD = 1,
        SSI263 = 2, // currently not selectable in libretro
        UNKNOWN
    };

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName);

    void writeAudio(const AudioSource selectedSource, const size_t fps);
    void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
} // namespace ra2
