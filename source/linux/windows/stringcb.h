#pragma once

#include "linux/windows/wincompat.h"
#include <cstddef>

HRESULT StringCbCopy(char * pszDest, const size_t cbDest, const char * pszSrc);
HRESULT StringCbCat(char * pszDest,const size_t cbDest, const char * pszSrc);
#define StringCbPrintf snprintf
