/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

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

/* Description: Save-state (snapshot) module
 *
 * Author: Copyright (c) 2004-2006 Tom Charlesworth
 */

#include "StdAfx.h"
#pragma  hdrstop

#define DEFAULT_SNAPSHOT_NAME "SaveState.aws"

bool g_bSaveStateOnExit = false;

static char g_szSaveStateFilename[MAX_PATH] = {0};

//-----------------------------------------------------------------------------

char* Snapshot_GetFilename()
{
	return g_szSaveStateFilename;
}

void Snapshot_SetFilename(char* pszFilename)
{
	if(*pszFilename)
		strcpy(g_szSaveStateFilename, (const char *) pszFilename);
	else
		strcpy(g_szSaveStateFilename, DEFAULT_SNAPSHOT_NAME);
}

//-----------------------------------------------------------------------------

void Snapshot_LoadState()
{
	char szMessage[32 + MAX_PATH];

	APPLEWIN_SNAPSHOT* pSS = (APPLEWIN_SNAPSHOT*) new char[sizeof(APPLEWIN_SNAPSHOT)];

	try
	{
		if(pSS == NULL)
			throw(0);

		memset(pSS, 0, sizeof(APPLEWIN_SNAPSHOT));

		//

		HANDLE hFile = CreateFile(	g_szSaveStateFilename,
									GENERIC_READ,
									0,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);

		if(hFile == INVALID_HANDLE_VALUE)
		{
			strcpy(szMessage, "File not found: ");
			strcpy(szMessage + strlen(szMessage), g_szSaveStateFilename);
			throw(0);
		}

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(	hFile,
								pSS,
								sizeof(APPLEWIN_SNAPSHOT),
								&dwBytesRead,
								NULL);

		CloseHandle(hFile);

		if(!bRes || (dwBytesRead != sizeof(APPLEWIN_SNAPSHOT)))
		{
			// File size wrong: probably because of version mismatch or corrupt file
			strcpy(szMessage, "File size mismatch");
			throw(0);
		}

		if(pSS->Hdr.dwTag != AW_SS_TAG)
		{
			strcpy(szMessage, "File corrupt");
			throw(0);
		}

		if(pSS->Hdr.dwVersion != MAKE_VERSION(1,0,0,1))
		{
			strcpy(szMessage, "Version mismatch");
			throw(0);
		}

		// TO DO: Verify checksum

		//
		// Reset all sub-systems
		MemReset();

		if (g_bApple2e)
			MemResetPaging();

		DiskReset();
		KeybReset();
		VideoResetState();
		MB_Reset();

		//
		// Apple2 uint
		//

		CpuSetSnapshot(&pSS->Apple2Unit.CPU6502);
		CommSetSnapshot(&pSS->Apple2Unit.Comms);
		JoySetSnapshot(&pSS->Apple2Unit.Joystick);
		KeybSetSnapshot(&pSS->Apple2Unit.Keyboard);
		SpkrSetSnapshot(&pSS->Apple2Unit.Speaker);
		VideoSetSnapshot(&pSS->Apple2Unit.Video);
		MemSetSnapshot(&pSS->Apple2Unit.Memory);

		//

		//
		// Slot4: Mockingboard
		MB_SetSnapshot(&pSS->Mockingboard1, 4);

		//
		// Slot5: Mockingboard
		MB_SetSnapshot(&pSS->Mockingboard2, 5);

		//
		// Slot6: Disk][
		DiskSetSnapshot(&pSS->Disk2, 6);
	}
	catch(int)
	{
		MessageBox(	g_hFrameWindow,
					szMessage,
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}

	delete [] pSS;
}

//-----------------------------------------------------------------------------

void Snapshot_SaveState()
{
	APPLEWIN_SNAPSHOT* pSS = (APPLEWIN_SNAPSHOT*) new char[sizeof(APPLEWIN_SNAPSHOT)];
	if(pSS == NULL)
	{
		// To do
		return;
	}

	memset(pSS, 0, sizeof(APPLEWIN_SNAPSHOT));

	pSS->Hdr.dwTag = AW_SS_TAG;
	pSS->Hdr.dwVersion = MAKE_VERSION(1,0,0,1);
	pSS->Hdr.dwChecksum = 0;	// TO DO

	//
	// Apple2 uint
	//

	pSS->Apple2Unit.UnitHdr.dwLength = sizeof(SS_APPLE2_Unit);
	pSS->Apple2Unit.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);

	CpuGetSnapshot(&pSS->Apple2Unit.CPU6502);
	CommGetSnapshot(&pSS->Apple2Unit.Comms);
	JoyGetSnapshot(&pSS->Apple2Unit.Joystick);
	KeybGetSnapshot(&pSS->Apple2Unit.Keyboard);
	SpkrGetSnapshot(&pSS->Apple2Unit.Speaker);
	VideoGetSnapshot(&pSS->Apple2Unit.Video);
	MemGetSnapshot(&pSS->Apple2Unit.Memory);

	//
	// Slot1: Empty
	pSS->Empty1.Hdr.UnitHdr.dwLength = sizeof(SS_CARD_EMPTY);
	pSS->Empty1.Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);
	pSS->Empty1.Hdr.dwSlot = 1;
	pSS->Empty1.Hdr.dwType = CT_Empty;

	//
	// Slot2: Empty
	pSS->Empty2.Hdr.UnitHdr.dwLength = sizeof(SS_CARD_EMPTY);
	pSS->Empty2.Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);
	pSS->Empty2.Hdr.dwSlot = 2;
	pSS->Empty2.Hdr.dwType = CT_Empty;

	//
	// Slot3: Empty
	pSS->Empty3.Hdr.UnitHdr.dwLength = sizeof(SS_CARD_EMPTY);
	pSS->Empty3.Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,0);
	pSS->Empty3.Hdr.dwSlot = 3;
	pSS->Empty3.Hdr.dwType = CT_Empty;

	//
	// Slot4: Mockingboard
	MB_GetSnapshot(&pSS->Mockingboard1, 4);

	//
	// Slot5: Mockingboard
	MB_GetSnapshot(&pSS->Mockingboard2, 5);

	//
	// Slot6: Disk][
	DiskGetSnapshot(&pSS->Disk2, 6);

	//

	HANDLE hFile = CreateFile(	g_szSaveStateFilename,
								GENERIC_WRITE,
								0,
								NULL,
								CREATE_ALWAYS,
								FILE_ATTRIBUTE_NORMAL,
								NULL);

	DWORD dwError = GetLastError();
	_ASSERT((dwError == 0) || (dwError == ERROR_ALREADY_EXISTS));

	if(hFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(	hFile,
								pSS,
								sizeof(APPLEWIN_SNAPSHOT),
								&dwBytesWritten,
								NULL);

		if(!bRes || (dwBytesWritten != sizeof(APPLEWIN_SNAPSHOT)))
			dwError = GetLastError();

		CloseHandle(hFile);
	}
	else
	{
		dwError = GetLastError();
	}

	_ASSERT((dwError == 0) || (dwError == ERROR_ALREADY_EXISTS));

	delete [] pSS;
}

//-----------------------------------------------------------------------------

void Snapshot_Startup()
{
	static bool bDone = false;

	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_LoadState();

	bDone = true;
}

void Snapshot_Shutdown()
{
	static bool bDone = false;

	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_SaveState();

	bDone = true;
}
