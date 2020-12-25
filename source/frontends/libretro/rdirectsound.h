#pragma once

#include <cstdlib>

namespace RDirectSound
{
  void writeAudio(const size_t ms);
  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
}
