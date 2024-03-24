#pragma once

#include <cstdlib>

#define REGVALUE_AUDIO_SPEAKER        "Speaker"
#define REGVALUE_AUDIO_MOCKINGBOARD   "Mockingboard"

namespace ra2
{
  enum class AudioSource
  {
    SPEAKER = 0,
    MOCKINGBOARD = 1,
    SSI263 = 2,           // currently not selectable in libretro
    UNKNOWN
  };

  void writeAudio(const AudioSource selectedSource, const size_t fps);
  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
}
