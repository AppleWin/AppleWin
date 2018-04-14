#pragma once

#include "linux/wincompat.h"
#include <string>

// Resources

HRSRC FindResource(void *, const std::string & filename, const char *);

// Frame

void FrameDrawDiskLEDS(HDC x);
void FrameDrawDiskStatus(HDC x);
void FrameRefreshStatus(int x, bool);

// Keyboard

BYTE    KeybGetKeycode ();
BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);

// Joystick

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);

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
