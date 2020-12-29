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
FILE* g_fh = NULL;

#ifdef _MSC_VER
#define LOG_FILENAME "AppleWin.log"
#else
// save to /tmp as otherwise it creates a file in the current folder which can be a bit everywhere
// especially if the program is installed to /usr
#define LOG_FILENAME "/tmp/AppleWin.log"
#endif


//---------------------------------------------------------------------------

void LogInit(void)
{
	if (g_fh)
		return;

	g_fh = fopen(LOG_FILENAME, "a+t");	    // Open log file (append & text mode)
	setvbuf(g_fh, NULL, _IONBF, 0);			// No buffering (so implicit fflush after every fprintf)
	CHAR aDateStr[80], aTimeStr[80];
	GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aDateStr, sizeof(aDateStr));
	GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aTimeStr, sizeof(aTimeStr));
	fprintf(g_fh, "*** Logging started: %s %s\n", aDateStr, aTimeStr);
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

void LogOutput(LPCTSTR format, ...)
{
	TCHAR output[256];

	va_list args;
	va_start(args, format);

	_vsntprintf(output, sizeof(output) - 1, format, args);
	output[sizeof(output) - 1] = 0;
	OutputDebugString(output);
}

//---------------------------------------------------------------------------

void LogFileOutput(LPCTSTR format, ...)
{
	if (!g_fh)
		return;

	TCHAR output[256];

	va_list args;
	va_start(args, format);

	_vsntprintf(output, sizeof(output) - 1, format, args);
	output[sizeof(output) - 1] = 0;
	fprintf(g_fh, "%s", output);
}
