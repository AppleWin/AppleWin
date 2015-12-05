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
void    KeybQueueKeypress (int,BOOL);
void    KeybToggleCapsLock ();
void    KeybToggleP8ACapsLock ();
void    KeybSetSnapshot_v1(const BYTE LastKey);
void    KeybSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    KeybLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
void    KeybGetSnapshot(BYTE& rLastKey);
void    KeybSetSnapshot(const BYTE LastKey);

BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall KbdAllow8Bit (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft); //For Pravets A/C only

extern bool g_bShiftKey;
extern bool g_bCtrlKey;
extern bool g_bAltKey;
