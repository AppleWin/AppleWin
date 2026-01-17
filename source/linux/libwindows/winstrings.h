#pragma once

#include "wincompat.h"

#include <cstddef>
#include <ctype.h>
#include <cstdarg>

void strcpy_s(char *dest, size_t size, const char *source);

#define _TRUNCATE ((size_t) - 1)

#define _strtoui64 strtoull
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define sscanf_s sscanf

inline bool IsCharLower(char ch)
{
    return isascii(ch) && islower(ch);
}

DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength);

LPSTR _strupr(LPSTR str);

errno_t strncpy_s(char *dest, size_t numberOfElements, const char *src, size_t count);

template <size_t size> errno_t strncpy_s(char (&dest)[size], const char *src, size_t count)
{
    return strncpy_s(dest, size, src, count);
}
