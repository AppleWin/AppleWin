/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Log
 *
 * Author: Nick Westgate
 */

#include "StdAfx.h"

#include "Log.h"

#include <time.h>

FILE* g_fh = NULL;

#ifdef _MSC_VER
#define LOG_FILENAME "AppleWin.log"
#else
// save to /tmp as otherwise it creates a file in the current folder which can be a bit everywhere
// especially if the program is installed to /usr
#define LOG_FILENAME "/tmp/AppleWin.log"
#endif


//---------------------------------------------------------------------------

inline std::string GetTimeStamp()
{
	time_t ltime;
	time(&ltime);
#ifdef _MSC_VER
	char ct[32];
	ctime_s(ct, sizeof(ct), &ltime);
#else
	char ctbuf[32];
	const char* ct = ctime_r(&ltime, ctbuf);
#endif
	return std::string(ct, 24);
}

void LogInit(void)
{
	if (g_fh)
		return;

	g_fh = fopen(LOG_FILENAME, "a+t");	    // Open log file (append & text mode)
	if (!g_fh)
	{
		LogOutput("Failed to open logfile '%s'\n", LOG_FILENAME);
		return;
	}

	setvbuf(g_fh, NULL, _IONBF, 0);			// No buffering (so implicit fflush after every fprintf)

	fprintf(g_fh, "*** Logging started: %s\n", GetTimeStamp().c_str());
}

void LogDone(void)
{
	if (!g_fh)
		return;

	fprintf(g_fh,"*** Logging ended\n\n");
	fclose(g_fh);
	g_fh = NULL;
}

//---------------------------------------------------------------------------

void LogOutput(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	OutputDebugString(StrFormatV(format, args).c_str());

	va_end(args);
}

//---------------------------------------------------------------------------

void LogFileOutput(const char* format, ...)
{
	if (!g_fh)
		return;

	va_list args;
	va_start(args, format);

	vfprintf(g_fh, format, args);

	va_end(args);
}
