#include "linux/interface.h"

void registerSoundBuffer(IDirectSoundBuffer * buffer)
{
}

void unregisterSoundBuffer(IDirectSoundBuffer * buffer)
{
}

// Resources

HRSRC FindResource(void *, const char * filename, const char *)
{
  return HRSRC();
}

// Keyboard

BYTE KeybGetKeycode() { return 0; }
BYTE KeybReadData() { return 0; }
BYTE KeybReadFlag() { return 0; }

// Joystick

BYTE JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }
BYTE JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }

// MessageBox

int MessageBox(HWND, const char * text, const char * caption, UINT type) { return 0; }

// Bitmap

HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename)
{
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  return 0;
}
