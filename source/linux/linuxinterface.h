#pragma once

#include "linux/windows/wincompat.h"
#include "linux/windows/dsound.h"

// Sound
void registerSoundBuffer(IDirectSoundBuffer * buffer);
void unregisterSoundBuffer(IDirectSoundBuffer * buffer);
