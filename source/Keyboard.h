#pragma once

enum	Keystroke_e {NOT_ASCII=0, ASCII};

void    ClipboardInitiatePaste();

void    KeybReset();
bool    KeybGetCapsStatus();
bool    KeybGetP8CapsStatus();
bool    KeybGetAltStatus();
bool    KeybGetCtrlStatus();
bool    KeybGetShiftStatus();
void    KeybUpdateCtrlShiftStatus();
BYTE    KeybGetKeycode ();
void    KeybQueueKeypress(WPARAM key, Keystroke_e bASCII);
void    KeybToggleCapsLock ();
void    KeybToggleP8ACapsLock ();
void    KeybAnyKeyDown(UINT message, WPARAM wparam, bool bIsExtended);
BYTE    KeybReadData (void);
BYTE    KeybReadFlag (void);
void    KeybSetSnapshot_v1(const BYTE LastKey);
void    KeybSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    KeybLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
