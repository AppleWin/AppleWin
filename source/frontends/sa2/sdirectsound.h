#pragma once

#include <SDL.h>


namespace SDirectSound
{
  void stop();
  void writeAudio();
  void setOptions(const int initialSilence);
}
