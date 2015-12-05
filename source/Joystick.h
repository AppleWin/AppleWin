#pragma once

enum JOYNUM {JN_JOYSTICK0=0, JN_JOYSTICK1};

enum JOY0CHOICE {J0C_DISABLED=0, J0C_JOYSTICK1, J0C_KEYBD_CURSORS, J0C_KEYBD_NUMPAD, J0C_MOUSE, J0C_MAX};
enum JOY1CHOICE {J1C_DISABLED=0, J1C_JOYSTICK2, J1C_KEYBD_CURSORS, J1C_KEYBD_NUMPAD, J1C_MOUSE, J1C_MAX};

enum {JOYSTICK_MODE_FLOATING=0, JOYSTICK_MODE_CENTERING};	// Joystick centering control

void    JoyInitialize();
BOOL    JoyProcessKey(int,BOOL,BOOL,BOOL);
void    JoyReset();
void    JoySetButton(eBUTTON,eBUTTONSTATE);
BOOL    JoySetEmulationType(HWND,DWORD,int, const bool bMousecardActive);
void    JoySetPosition(int,int,int,int);
void    JoyUpdateButtonLatch(const UINT nExecutionPeriodUsec);
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
void    JoySetSnapshot_v1(const unsigned __int64 JoyCntrResetCycle);
void    JoySaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    JoyLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
void    JoyGetSnapshot(unsigned __int64& rJoyCntrResetCycle, short* pJoystick0Trim, short* pJoystick1Trim);
void    JoySetSnapshot(const unsigned __int64 JoyCntrResetCycle, const short* pJoystick0Trim, const short* pJoystick1Trim);

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
