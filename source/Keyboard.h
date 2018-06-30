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
void    KeybAnyKeyDown(UINT message, WPARAM wparam, bool bIsExtended);
BYTE    KeybReadData (void);
BYTE    KeybReadFlag (void);
void    KeybSetSnapshot_v1(const BYTE LastKey);
void    KeybSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    KeybLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

extern bool g_bShiftKey;
extern bool g_bCtrlKey;
extern bool g_bAltKey;
