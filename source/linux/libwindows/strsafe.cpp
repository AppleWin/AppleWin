#include "strsafe.h"
#include <cstring>

HRESULT StringCbCopy(char * pszDest, const size_t cbDest, const char * pszSrc)
{
  strncpy(pszDest, pszSrc, cbDest - 1);
  pszDest[cbDest - 1] = '\0';
  return 0;
}

HRESULT StringCbCat(char * pszDest, const size_t cbDest, const char * pszSrc)
{
  strncat(pszDest, pszSrc, cbDest - strlen(pszDest) - 1);
  pszDest[cbDest - 1] = '\0';
  return 0;
}
