#pragma once

#include "linux/windows/handles.h"

typedef void * HCURSOR;

#define IDC_WAIT "IDC_WAIT"

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR SetCursor(HCURSOR hCursor);

typedef VOID    (CALLBACK *TIMERPROC)(HWND,UINT,UINT_PTR,DWORD);

UINT_PTR SetTimer(HWND,UINT_PTR,UINT,TIMERPROC);
BOOL KillTimer(HWND hWnd, UINT uIDEvent);
