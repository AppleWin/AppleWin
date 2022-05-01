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

LPSTR _strupr( LPSTR str )
{
  LPSTR ret = str;
  for ( ; *str; str++)
    *str = toupper(*str);
  return ret;
}

errno_t strncpy_s(char * dest, size_t numberOfElements, const char *src, size_t count)
{
  if (!count)
  {
    if(dest && numberOfElements)
    {
      *dest = 0;
    }
    return 0;
  }

  size_t end;
  if (count != _TRUNCATE && count < numberOfElements)
  {
    end = count;
  }
  else
  {
    end = numberOfElements - 1;
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
