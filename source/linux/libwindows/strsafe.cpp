#include "strsafe.h"
#include <cstring>

HRESULT StringCbCopy(char *pszDest, const size_t cbDest, const char *pszSrc)
{
    strncpy(pszDest, pszSrc, cbDest - 1);
    pszDest[cbDest - 1] = '\0';
    return 0;
}
