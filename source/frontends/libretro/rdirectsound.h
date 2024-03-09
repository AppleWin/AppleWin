#pragma once

#include <cstdlib>

#define REGVALUE_AUDIO_SPEAKER        "Speaker"
#define REGVALUE_AUDIO_MOCKINGBOARD   "Mockingboard"

namespace ra2
{
  enum class eAudioSource
  {
    SPEAKER = 0,
    MOCKINGBOARD = 1,
    SSI263 = 2,           // currently not selectable via libretro
    UNKNOWN
  };

  void writeAudio(const eAudioSource selectedSource, const size_t fps);
  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
}
