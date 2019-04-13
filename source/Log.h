#pragma once

#ifndef _VC71	// __VA_ARGS__ not supported on MSVC++ .NET 7.x
	#ifdef _DEBUG
		#define LOG(format, ...) LogOutput(format, __VA_ARGS__)
	#else
		#define LOG(...)
	#endif
#endif

extern FILE* g_fh;	// Filehandle for log file

void LogInit(void);
void LogDone(void);
void LogOutput(LPCTSTR format, ...);
void LogFileOutput(LPCTSTR format, ...);
