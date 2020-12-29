#pragma once

#include "linux/windows/wincompat.h"
#include "linux/windows/dsound.h"
#include "linux/windows/resources.h"
#include "linux/windows/bitmap.h"

#include <string>

// Resources
HRSRC FindResource(void *, const char * filename, const char *);

// Bitmap
HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename);
LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits);

// MessageBox
int MessageBox(HWND, const char * text, const char * caption, UINT type);

// Sound
void registerSoundBuffer(IDirectSoundBuffer * buffer);
void unregisterSoundBuffer(IDirectSoundBuffer * buffer);
