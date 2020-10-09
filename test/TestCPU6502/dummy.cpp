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

// Frame

void FrameDrawDiskLEDS(HDC x) { }
void FrameDrawDiskStatus(HDC x) { }
void FrameRefreshStatus(int x, bool) { }

// Keyboard

BYTE KeybGetKeycode () { return 0; }
BYTE KeybReadData() { return 0; }
BYTE KeybReadFlag() { return 0; }

// Joystick

BYTE JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }
BYTE JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }
void JoyResetPosition(ULONG nCyclesLeft) { }

// Registry

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars) { return FALSE; }
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value) { return FALSE; }
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value) { return FALSE; }
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer) { }
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) { }

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
