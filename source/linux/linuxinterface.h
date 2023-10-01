#pragma once

class IDirectSoundBuffer;

// Sound
IDirectSoundBuffer * iCreateDirectSoundBuffer(LPCDSBUFFERDESC lpcDSBufferDesc);

void unregisterSoundBuffer(IDirectSoundBuffer * buffer);
