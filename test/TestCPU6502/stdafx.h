// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _MSC_VER

#include <stdio.h>
#include <tchar.h>

#include <windows.h>

#include <stdint.h> // cleanup WORD DWORD -> uint16_t uint32_t

#include <string>

#else

#include <cstring>
#include <cstdlib>
#include "windows.h"
#include <string>

#endif
