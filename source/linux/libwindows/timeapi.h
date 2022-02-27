#pragma once

#include "wincompat.h"

#include <ctime>

#define _tzset tzset
errno_t ctime_s(char * buf, size_t size, const time_t *time);

#define LOCALE_SYSTEM_DEFAULT 0x0800

typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME;

int GetDateFormat(LCID Locale, DWORD dwFlags, CONST SYSTEMTIME *lpDate, LPCSTR lpFormat, LPSTR lpDateStr, int cchDate);
int GetTimeFormat(LCID Locale, DWORD dwFlags, CONST SYSTEMTIME *lpTime, LPCSTR lpFormat, LPSTR lpTimeStr, int cchTime);

DWORD timeGetTime();
DWORD GetTickCount();
void GetLocalTime(SYSTEMTIME *t);
