#include "winbase.h"

#include <errno.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sstream>


DWORD       WINAPI GetLastError(void)
{
  return errno;
}

void DeleteCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void InitializeCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void EnterCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void LeaveCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

BOOL SetCurrentDirectory(LPCSTR path)
{
  const int res = chdir(path);
  return res == 0;
}


void OutputDebugString(const char * str)
{
  std::cerr << str;
}

void ExitProcess(int status)
{
  std::ostringstream buffer("ExitProcess: ");
  buffer << status;

  throw std::runtime_error(buffer.str());
}

DWORD       WINAPI WaitForMultipleObjects(DWORD,const HANDLE*,BOOL,DWORD)
{
  return WAIT_FAILED;
}

DWORD       WINAPI WaitForSingleObject(const HANDLE,DWORD)
{
  return WAIT_FAILED;
}

BOOL        WINAPI GetExitCodeThread(HANDLE,LPDWORD code)
{
  *code = 0;
  return TRUE;
}

BOOL        WINAPI SetEvent(HANDLE)
{
  return TRUE;
}

VOID        WINAPI Sleep(DWORD ms)
{
  std::this_thread::sleep_for (std::chrono::milliseconds(ms));
}

BOOL        WINAPI SetThreadPriority(HANDLE,INT)
{
  return TRUE;
}

HANDLE      WINAPI CreateEvent(LPSECURITY_ATTRIBUTES,BOOL,BOOL,LPCSTR)
{
  return INVALID_HANDLE_VALUE;
}

HANDLE      WINAPI CreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)
{
  return INVALID_HANDLE_VALUE;
}

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER*counter)
{
  const auto now = std::chrono::steady_clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  counter->QuadPart = ms.count();
  return TRUE;
}

HANDLE CreateSemaphore(
  LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
  LONG                  lInitialCount,
  LONG                  lMaximumCount,
  LPCSTR                lpName
)
{
  return INVALID_HANDLE_VALUE;
}
