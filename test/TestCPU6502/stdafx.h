// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32

#include <stdio.h>

#include <windows.h>

#include <stdint.h> // cleanup WORD DWORD -> uint16_t uint32_t
#include <crtdbg.h>

#include <string>

#else

#include <cstring>
#include <cstdlib>
#include "windows.h"
#include <string>

#endif
