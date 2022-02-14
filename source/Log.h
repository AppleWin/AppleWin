#pragma once

#include <cstdio>

#include "StrFormat.h"

#ifndef _VC71	// __VA_ARGS__ not supported on MSVC++ .NET 7.x
	#ifdef _DEBUG
		#define LOG(format, ...) LogOutput(format, __VA_ARGS__)
	#else
		#define LOG(...)
	#endif
#endif

extern FILE* g_fh;	// File handle for log file

void LogInit(void);
void LogDone(void);

void LogOutput(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
void LogFileOutput(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
