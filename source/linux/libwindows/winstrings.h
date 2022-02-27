#pragma once

#include "wincompat.h"
#include "misc.h"

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
#define _tcsncmp strncmp
#define _tcsncat strncat

inline bool IsCharLower(char ch) {
        return isascii(ch) && islower(ch);
}

inline bool IsCharUpper(char ch) {
        return isascii(ch) && isupper(ch);
}

DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength);

LPSTR _strupr( LPSTR str );

// copied from
// https://github.com/wine-mirror/wine/blob/1d178982ae5a73b18f367026c8689b56789c39fd/dlls/msvcrt/heap.c#L833
template <size_t size>
errno_t strncpy_s(char (&dest)[size], const char *src, size_t count)
{
  size_t end;
  if (count != _TRUNCATE && count < size)
  {
    end = count;
  }
  else
  {
    end = size - 1;
  }

  size_t i;
  for (i = 0; i < end && src[i]; i++)
  {
    dest[i] = src[i];
  }

  if (!src[i] || end == count || count == _TRUNCATE)
  {
    dest[i] = '\0';
    return 0;
  }

  dest[0] = '\0';
  return EINVAL;
}
