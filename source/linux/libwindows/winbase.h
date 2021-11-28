#pragma once

#include "wincompat.h"
#include "winhandles.h"

#include <cstddef>

DWORD       WINAPI GetLastError(void);
void DeleteCriticalSection(CRITICAL_SECTION * criticalSection);
void InitializeCriticalSection(CRITICAL_SECTION * criticalSection);
void EnterCriticalSection(CRITICAL_SECTION * criticalSection);
void LeaveCriticalSection(CRITICAL_SECTION * criticalSection);
void OutputDebugString(const char * str);
BOOL WINAPI SetCurrentDirectory(LPCSTR path);

#define INFINITE      0xFFFFFFFF
#define WAIT_OBJECT_0           0
#define WAIT_FAILED             0xffffffff
#define STATUS_PENDING          0x00000103
#define STILL_ACTIVE            STATUS_PENDING


#define THREAD_BASE_PRIORITY_LOWRT  15
#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT

typedef DWORD (CALLBACK *LPTHREAD_START_ROUTINE)(LPVOID);
typedef size_t SIZE_T;

BOOL        WINAPI SetThreadPriority(HANDLE,INT);
DWORD       WINAPI WaitForSingleObject(const HANDLE,DWORD);
DWORD       WINAPI WaitForMultipleObjects(DWORD,const HANDLE*,BOOL,DWORD);
BOOL        WINAPI GetExitCodeThread(HANDLE,LPDWORD);
BOOL        WINAPI SetEvent(HANDLE);
VOID        WINAPI Sleep(DWORD);
HANDLE      WINAPI CreateEvent(LPSECURITY_ATTRIBUTES,BOOL,BOOL,LPCSTR);
HANDLE      WINAPI CreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER;

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER*);

HANDLE CreateSemaphore(
  LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
  LONG                  lInitialCount,
  LONG                  lMaximumCount,
  LPCSTR                lpName
);
