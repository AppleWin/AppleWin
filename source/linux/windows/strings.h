#pragma once

#include "linux/windows/wincompat.h"

#include <cstddef>
#include <ctype.h>
#include <cstdarg>

void strcpy_s(char * dest, size_t size, const char * source);

int vsnprintf_s(
   char *buffer,
   size_t sizeOfBuffer,
   size_t count,
   const char *format,
   va_list argptr
);

#define _TRUNCATE ((size_t)-1)

#define sprintf_s snprintf

#define _strdup strdup
#define _strtoui64 strtoull
#define _vsntprintf vsnprintf
#define _tcsrchr strrchr
#define _tcsncpy strncpy
#define _tcslen     strlen
#define _tcscmp strcmp
#define _tcsicmp _stricmp
#define _stricmp strcasecmp
#define _tcschr strchr
#define _tcsstr strstr
#define _tcscpy     strcpy
#define _tcstol strtol
#define _tcstoul strtoul
#define _snprintf snprintf
#define wsprintf sprintf
#define sscanf_s sscanf

inline bool IsCharLower(char ch) {
	return isascii(ch) && islower(ch);
}

inline bool IsCharUpper(char ch) {
	return isascii(ch) && isupper(ch);
}

DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength);
