#pragma once

#include "wincompat.h"
#include "winhandles.h"

#define CBR_9600 9600

#define WM_USER 0x0400
#ifndef INVALID_SOCKET
#define INVALID_SOCKET             (SOCKET)(~0)
#endif

#define MB_OK			0x00000000
#define MB_OKCANCEL		0x00000001
#define MB_ICONEXCLAMATION	0x00000030
#define MB_YESNO		0x00000004
#define MB_YESNOCANCEL		0x00000003
#define MB_ICONHAND		0x00000010
#define MB_ICONQUESTION		0x00000020
#define MB_ICONASTERISK		0x00000040
#define MB_SETFOREGROUND	0x00010000
#define MB_ICONSTOP		MB_ICONHAND
#define MB_ICONWARNING		MB_ICONEXCLAMATION
#define MB_ICONINFORMATION	MB_ICONASTERISK

#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

#define EINVAL          22

BOOL WINAPI PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
int MessageBox(HWND, const char *, const char *, UINT);

void OutputDebugString(const char * str);

void DeleteCriticalSection(CRITICAL_SECTION * criticalSection);
void InitializeCriticalSection(CRITICAL_SECTION * criticalSection);
void EnterCriticalSection(CRITICAL_SECTION * criticalSection);
void LeaveCriticalSection(CRITICAL_SECTION * criticalSection);

void ExitProcess(int);
