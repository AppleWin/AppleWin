#pragma once

#if _DEBUG
// __VA_ARGS__ not supported on MSVC++ .NET 7.x
//    #define LOG(format, ...) LogOutput(format, __VA_ARGS__)
    #define LOG(format, params) LogOutput(format, params)
#else
//    #define LOG(...)
    #define LOG(format, params)
#endif

extern void LogOutput(LPCTSTR format, ...);
