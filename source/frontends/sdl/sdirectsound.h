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
    std::string streamName;
    int channels = 0;

    // in seconds
    // float to work with ImGui.
    float buffer = 0.0;
    float size = 0.0;
    float volume = 0.0;

    size_t numberOfUnderruns = 0;
  };

  std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pStreamName);

  void resetAudioUnderruns();
  void printAudioInfo();
  std::vector<SoundInfo> getAudioInfo();

  void setAudioOptions(const common2::EmulatorOptions & options);
}
