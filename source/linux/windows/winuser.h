#pragma once

#include "linux/windows/handles.h"

typedef void * HCURSOR;

#define IDC_WAIT "IDC_WAIT"
#define CB_ERR              (-1)
#define CB_ADDSTRING             0x0143
#define CB_RESETCONTENT          0x014b
#define CB_SETCURSEL             0x014e

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR SetCursor(HCURSOR hCursor);

typedef VOID    (CALLBACK *TIMERPROC)(HWND,UINT,UINT_PTR,DWORD);

UINT_PTR SetTimer(HWND,UINT_PTR,UINT,TIMERPROC);
BOOL KillTimer(HWND hWnd, UINT uIDEvent);
HWND        WINAPI GetDlgItem(HWND,INT);
LRESULT     WINAPI SendMessage(HWND,UINT,WPARAM,LPARAM);
