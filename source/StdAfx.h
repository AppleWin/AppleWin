#ifdef _WIN32

#ifdef __MINGW32__
#define STRSAFE_NO_DEPRECATE
#endif

#include <crtdbg.h>
// <strmif.h> has the correct IReferenceClock definition that works for both x86 and x64,
// whereas the alternative definition in <dsound.h> is incorrect for x64. (out of
// maintenance perhaps)
// <strmif.h> *must* be included before <dsound.h> for x64 to work.
#include <strmif.h>
#include <dsound.h>
#include <dshow.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h> // cleanup WORD DWORD -> uint16_t uint32_t

#include <windows.h>
#include <winuser.h> // WM_MOUSEWHEEL
#include <shlwapi.h>
#include <strsafe.h>
#include <commctrl.h>
#include <ddraw.h>
#include <htmlhelp.h>
#include <assert.h>
#include <winsock.h>

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

#ifndef __MINGW32__ 
#define USE_SPEECH_API
#endif

#ifdef __MINGW32__
#define SIZE_T_FMT "llu"
#else
#define SIZE_T_FMT "zu"
#endif

#else // !_WIN32

#include <cmath>
#include <map>
#include <stack>
#include <stdexcept>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <cassert>
#include <memory>

// NOTE: this is a local version of windows.h with aliases for windows functions when not
//       building in a windows environment (!_WIN32)
#include "windows.h"

//#define USE_SPEECH_API

#define SIZE_T_FMT "zu"

#endif // _WIN32
