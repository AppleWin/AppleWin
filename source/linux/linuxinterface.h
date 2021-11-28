#pragma once

class IDirectSoundBuffer;

// Sound
void registerSoundBuffer(IDirectSoundBuffer * buffer);
void unregisterSoundBuffer(IDirectSoundBuffer * buffer);
