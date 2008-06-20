#pragma once

void    ClipboardInitiatePaste();

void    KeybReset();
bool    KeybGetAltStatus();
bool    KeybGetCapsStatus();
bool    KeybGetP8CapsStatus();
bool    KeybGetCtrlStatus();
bool    KeybGetShiftStatus();
bool    KeybGetCapsAllowed(); //For Pravets8A/C only
void    KeybUpdateCtrlShiftStatus();
BYTE    KeybGetKeycode ();
DWORD   KeybGetNumQueries ();
void    KeybQueueKeypress (int,BOOL);
void    KeybToggleCapsLock ();
void    KeybToggleP8ACapsLock ();
DWORD   KeybGetSnapshot(SS_IO_Keyboard* pSS);
DWORD   KeybSetSnapshot(SS_IO_Keyboard* pSS);

BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall KbdAllow8Bit (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft); //For Pravets A/C only
