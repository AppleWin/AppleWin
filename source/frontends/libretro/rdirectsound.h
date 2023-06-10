#pragma once

#include <cstdlib>

namespace ra2
{
  void writeAudio(const size_t channels, const size_t fps);
  void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely);
}
