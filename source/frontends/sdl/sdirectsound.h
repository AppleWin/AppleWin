#pragma once

#include <vector>

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  struct SoundInfo
  {
    bool running = false;
    int channels = 0;

    // in seconds
    // float to work with ImGui.
    float buffer = 0.0;
    float size = 0.0;
    float volume = 0.0;

    size_t numberOfUnderruns = 0;
  };

  void resetAudioUnderruns();
  void printAudioInfo();
  std::vector<SoundInfo> getAudioInfo();

  void setAudioOptions(const common2::EmulatorOptions & options);
}
