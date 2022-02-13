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
typedef INT8 int8_t;
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
#endif

#include <windows.h>
#include <winuser.h> // WM_MOUSEWHEEL
#include <shlwapi.h>
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
#include <cstdarg>

// SM_CXPADDEDBORDER is not supported on 2000 & XP:
// http://msdn.microsoft.com/en-nz/library/windows/desktop/ms724385(v=vs.85).aspx
#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 92
#endif

#define USE_SPEECH_API

#if (defined(_MSC_VER) && (_MSC_VER < 1900)) || \
    (!defined(_MSC_VER) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L))
#ifdef _WIN64
#define SIZE_T_FMT "llu"
#define PTRDIFF_T_FMT "lld"
#else
#define SIZE_T_FMT "lu"
#define PTRDIFF_T_FMT "ld"
#endif
#else
#define SIZE_T_FMT "zu"
#define PTRDIFF_T_FMT "td"
#endif
