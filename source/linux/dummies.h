#pragma once

#include <stdarg.h>

#define HD_LED 1

void DeleteCriticalSection(CRITICAL_SECTION * criticalSection);
void InitializeCriticalSection(CRITICAL_SECTION * criticalSection);
void EnterCriticalSection(CRITICAL_SECTION * criticalSection);
void LeaveCriticalSection(CRITICAL_SECTION * criticalSection);
void OutputDebugString(const char * str);
int MessageBox(HWND, const char *, const char *, UINT);

HWND GetDesktopWindow();
void ExitProcess(int);
