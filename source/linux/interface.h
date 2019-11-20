#pragma once

#include "linux/wincompat.h"
#include <string>

// Resources

HRSRC FindResource(void *, const std::string & filename, const char *);
HBITMAP LoadBitmap(HINSTANCE hInstance, const std::string & filename);
LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits);
BOOL DeleteObject(HGDIOBJ ho);


// Frame

void FrameDrawDiskLEDS(HDC x);
void FrameDrawDiskStatus(HDC x);
void FrameRefreshStatus(int x, bool);

// Keyboard

BYTE    KeybGetKeycode ();
BYTE KeybReadData();
BYTE KeybReadFlag();

// Joystick

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
void JoyResetPosition(ULONG uExecutedCycles);

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);

// Registry

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);

// MessageBox

int MessageBox(HWND, const char * text, const char * caption, UINT type);
