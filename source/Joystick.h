#pragma once

#include "Common.h"
enum JOYNUM {JN_JOYSTICK0=0, JN_JOYSTICK1};

enum JOY0CHOICE {J0C_DISABLED=0, J0C_JOYSTICK1, J0C_KEYBD_CURSORS, J0C_KEYBD_NUMPAD, J0C_MOUSE, J0C_MAX};
enum JOY1CHOICE {J1C_DISABLED=0, J1C_JOYSTICK2, J1C_KEYBD_CURSORS, J1C_KEYBD_NUMPAD, J1C_MOUSE, J1C_JOYSTICK1_THUMBSTICK2, J1C_MAX};

enum {JOYSTICK_MODE_FLOATING=0, JOYSTICK_MODE_CENTERING};	// Joystick centering control

void    JoyInitialize();
BOOL    JoyProcessKey(int,bool,bool,bool);
void    JoyReset();
void    JoySetButton(eBUTTON,eBUTTONSTATE);
BOOL    JoySetEmulationType(HWND,DWORD,int, const bool bMousecardActive);
void    JoySetPosition(int,int,int,int);
BOOL    JoyUsingMouse();
BOOL    JoyUsingKeyboard();
BOOL    JoyUsingKeyboardCursors();
BOOL    JoyUsingKeyboardNumpad();
void    JoyDisableUsingMouse();
void    JoySetJoyType(UINT num, DWORD type);
DWORD   JoyGetJoyType(UINT num);
void    JoySetTrim(short nValue, bool bAxisX);
short   JoyGetTrim(bool bAxisX);
void	JoyportControl(const UINT uControl);
void	JoySetHookAltKeys(bool hook);
void	JoySetButtonVirtualKey(UINT button, UINT virtKey);
void    JoySaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    JoyLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
void JoyResetPosition(ULONG nExecutedCycles);
