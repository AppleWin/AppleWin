#pragma once

#if _DEBUG
    #define LOG(format, ...) LogOutput(format, __VA_ARGS__)
#else
    #define LOG(...)
#endif

extern void LogOutput(LPCTSTR format, ...);
