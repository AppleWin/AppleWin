#pragma once

void    ClipboardInitiatePaste();

void    KeybReset();
void    KeybGetCapsStatus (BOOL *);
bool    KeybGetShiftStatus();
bool    KeybGetCtrlStatus();
void    KeybUpdateCtrlShiftStatus();
BYTE    KeybGetKeycode ();
DWORD   KeybGetNumQueries ();
void    KeybQueueKeypress (int,BOOL);
void    KeybToggleCapsLock ();
DWORD   KeybGetSnapshot(SS_IO_Keyboard* pSS);
DWORD   KeybSetSnapshot(SS_IO_Keyboard* pSS);

BYTE __stdcall KeybReadData (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall KeybReadFlag (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
