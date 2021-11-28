#include "winstrings.h"

#include <cstring>
#include <cstdio>

// make all chars in buffer lowercase
DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength)
{
  for (CHAR * c = lpsz; c != lpsz + cchLength; ++c)
  {
    *c = tolower(*c);
  }
  return cchLength;
}

void strcpy_s(char * dest, size_t size, const char * source)
{
  strncpy(dest, source, size);
}

int vsnprintf_s(char *buffer, size_t sizeOfBuffer, size_t count, const char *format, va_list argptr)
{
  // is this even right?
  const int res = vsnprintf(buffer, sizeOfBuffer, format, argptr);
  buffer[sizeOfBuffer - 1] = 0;
  return res;
}

LPSTR _strupr( LPSTR str )
{
  LPSTR ret = str;
  for ( ; *str; str++)
    *str = toupper(*str);
  return ret;
}
