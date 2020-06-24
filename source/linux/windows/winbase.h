#pragma once

#include "linux/windows/wincompat.h"
#include "linux/windows/handles.h"

DWORD       WINAPI GetLastError(void);
void DeleteCriticalSection(CRITICAL_SECTION * criticalSection);
void InitializeCriticalSection(CRITICAL_SECTION * criticalSection);
void EnterCriticalSection(CRITICAL_SECTION * criticalSection);
void LeaveCriticalSection(CRITICAL_SECTION * criticalSection);
void OutputDebugString(const char * str);
