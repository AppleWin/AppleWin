#include "linux/windows/strings.h"

#include <cstring>

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
