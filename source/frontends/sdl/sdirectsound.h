#pragma once

#include <string>
#include <vector>
#include <memory>

class SoundBuffer;

namespace common2
{
    struct EmulatorOptions;
}

namespace sa2
{

    struct SoundInfo
    {
        bool running = false;
        std::string voiceName;
        int channels = 0;

        // in seconds
        // float to work with ImGui.
        float buffer = 0.0;
        float size = 0.0;
        float volume = 0.0;

        size_t numberOfUnderruns = 0;
    };

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName);

    void resetAudioUnderruns();
    void printAudioInfo();
    std::vector<SoundInfo> getAudioInfo();

    void setAudioOptions(const common2::EmulatorOptions &options);
} // namespace sa2
