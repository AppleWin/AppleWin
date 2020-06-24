#include "linux/windows/winbase.h"

#include <errno.h>
#include <iostream>

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

void OutputDebugString(const char * str)
{
  std::cerr << str << std::endl;
}

void ExitProcess(int status)
{
  exit(status);
}
