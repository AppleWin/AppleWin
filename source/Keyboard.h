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
void    KeybSpecialKeyTransition(UINT message, WPARAM wparam);
void    KeybSetSnapshot_v1(const BYTE LastKey);
void    KeybSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    KeybLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
BYTE __stdcall KbdAllow8Bit (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles); //For Pravets A/C only

extern bool g_bShiftKey;
extern bool g_bCtrlKey;
extern bool g_bAltKey;
