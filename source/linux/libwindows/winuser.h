#pragma once

#include "winhandles.h"

typedef void *HCURSOR;

#define IDC_WAIT "IDC_WAIT"
#define CB_ERR (-1)
#define CB_ADDSTRING 0x0143
#define CB_RESETCONTENT 0x014b
#define CB_SETCURSEL 0x014e

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR SetCursor(HCURSOR hCursor);

typedef VOID(CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

UINT_PTR SetTimer(HWND, UINT_PTR, UINT, TIMERPROC);
BOOL KillTimer(HWND hWnd, UINT uIDEvent);
HWND WINAPI GetDlgItem(HWND, INT);
LRESULT WINAPI SendMessage(HWND, UINT, WPARAM, LPARAM);
void WINAPI PostQuitMessage(INT);

#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_OEM_3 0xC0 // '`~' for US
#define VK_OEM_8 0xDF

#define KF_EXTENDED 0x0100
