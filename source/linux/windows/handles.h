#pragma once

#include "linux/windows/wincompat.h"

struct CHANDLE
{
  virtual ~CHANDLE() {}
};

typedef CHANDLE * HANDLE;
#define INVALID_HANDLE_VALUE     ((HANDLE)~(ULONG_PTR)0)

typedef HANDLE HGLOBAL;

typedef void * HWD;
typedef void * HDC;
typedef void * HINSTANCE;
typedef void * LPARAM;
typedef void * WPARAM;
typedef void * SOCKET;
typedef void * CRITICAL_SECTION;
typedef void * LPDIRECTDRAW;
typedef void * LPOVERLAPPED;
typedef void * OVERLAPPED;
typedef void * LPSECURITY_ATTRIBUTES;
typedef void * HSEMAPHORE;


BOOL CloseHandle(HANDLE hObject);
