#include "linux/windows/winuser.h"

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName)
{
  return nullptr;
}

HCURSOR SetCursor(HCURSOR hCursor)
{
  return nullptr;
}

UINT_PTR SetTimer(HWND,UINT_PTR,UINT,TIMERPROC)
{
  return NULL;
}

BOOL KillTimer(HWND hWnd, UINT uIDEvent)
{
  return TRUE;
}

HWND        WINAPI GetDlgItem(HWND,INT)
{
  return nullptr;
}

LRESULT     WINAPI SendMessage(HWND,UINT,WPARAM,LPARAM)
{
  return 0;
}
