#include "linux/interface.h"

// Resources

HRSRC FindResource(void *, const std::string & filename, const char *)
{
  return HRSRC();
}

// Frame

void FrameDrawDiskLEDS(HDC x) { }
void FrameDrawDiskStatus(HDC x) { }
void FrameRefreshStatus(int x, bool) { }

// Keyboard

BYTE    KeybGetKeycode () { return 0; }
BYTE KeybReadData() { return 0; }
BYTE KeybReadFlag() { return 0; }

// Joystick

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }
void JoyResetPosition(ULONG nCyclesLeft) { }

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) { return 0; }

// Registry

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars) { return FALSE; }
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value) { return FALSE; }
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value) { return FALSE; }
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer) { }
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) { }

// MessageBox

int MessageBox(HWND, const char * text, const char * caption, UINT type) { return 0; }

// Bitmap

HBITMAP LoadBitmap(HINSTANCE hInstance, const std::string & filename)
{
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  return 0;
}

BOOL DeleteObject(HGDIOBJ ho)
{
  return TRUE;
}
