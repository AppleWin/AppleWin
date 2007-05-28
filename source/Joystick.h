#pragma once

enum JOYNUM {JN_JOYSTICK0=0, JN_JOYSTICK1};

extern DWORD      joytype[2];

void    JoyInitialize ();
BOOL    JoyProcessKey (int,BOOL,BOOL,BOOL);
void    JoyReset ();
void    JoySetButton (int,BOOL);
BOOL    JoySetEmulationType (HWND,DWORD,int);
void    JoySetPosition (int,int,int,int);
void    JoyUpdatePosition ();
BOOL    JoyUsingMouse ();
void    JoySetTrim(short nValue, bool bAxisX);
short   JoyGetTrim(bool bAxisX);
DWORD   JoyGetSnapshot(SS_IO_Joystick* pSS);
DWORD   JoySetSnapshot(SS_IO_Joystick* pSS);

BYTE __stdcall JoyReadButton (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall JoyReadPosition (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall JoyResetPosition (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
