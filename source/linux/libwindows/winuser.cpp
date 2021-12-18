#include "winuser.h"

#include <sstream>

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
  return 0;
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

void        WINAPI PostQuitMessage(INT status)
{
  std::ostringstream buffer("PostQuitMessage: ");
  buffer << status;

  throw std::runtime_error(buffer.str());
}
