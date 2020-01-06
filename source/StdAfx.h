//#define WIN32_LEAN_AND_MEAN

// Required for Win98/ME support:
// . See: http://support.embarcadero.com/article/35754
// . "GetOpenFileName() fails under Windows 95/98/NT/ME due to incorrect OPENFILENAME structure size"
#define _WIN32_WINNT 0x0400
#define WINVER 0x500

// Mouse Wheel is not supported on Win95.
// If we didn't care about supporting Win95 (compile/run-time errors)
// we would just define the minimum windows version to support.
// #define _WIN32_WINDOWS  0x0401
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

// Not needed in VC7.1, but needed in VC Express
#include <tchar.h>

#include <crtdbg.h>
#include <dsound.h>
#include <dshow.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if _MSC_VER >= 1600	// <stdint.h> supported from VS2010 (cl.exe v16.00)
#include <stdint.h> // cleanup WORD DWORD -> uint16_t uint32_t
#else
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
#endif

#include <windows.h>
#include <winuser.h> // WM_MOUSEWHEEL
#include <strsafe.h>
#include <commctrl.h>
#include <ddraw.h>
#include <htmlhelp.h>
#include <assert.h>

#include <algorithm>
#include <map>
#include <queue>
#include <stack>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

// SM_CXPADDEDBORDER is not supported on 2000 & XP:
// http://msdn.microsoft.com/en-nz/library/windows/desktop/ms724385(v=vs.85).aspx
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif

#define USE_SPEECH_API
