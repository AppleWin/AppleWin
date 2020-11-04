/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Determination and check of unique disk signatures
 *
 * Author: Henri Asseily
 *
 * Discoverability of the currently running program
 * To calculate the program signature we will CRC32 the last 4 modified pages of mainmem
 * and we will match each signature in turn to a file of known signatures.
 * This will give us the canonical signature and the name for the running program
 *
 */

#include "StdAfx.h"

#include "Applewin.h"
#include "Signatures.h"
#include "Memory.h"
#include "Frame.h"
#include "Log.h"
#include "Util_MemoryTextFile.h"	// load boot disk signatures
#include "zlib.h"					// crc32
#include "gamelink/gamelink.h"		// Select allocator
#include <map>

static UINT8 g_uMemPagesWrittenCount = 0;									// The number of main memory pages written until the second call to IORead_C00x
static UINT32 g_MemPagesWrittenFIFO[PROG_SIG_LEN] = { 0x08 };				// Tracks the last PRG_SIG_LEN memory pages written
static UINT32 g_PagesCRC[PROG_SIG_LEN] = { 0 };								// CRC of the last PRG_SOG_LEN pages
static std::map< std::string, std::string > g_mBootDisksSignatures = {};	// Map of signatures to program names (loaded from file)

UINT calculateMemPageSig(UINT crc, UINT8 pageNumber);

std::string runningProgramName()
{
	return g_pProgramName;
}

void addPageToSigCalculation(UINT page)
{
	g_MemPagesWrittenFIFO[g_uMemPagesWrittenCount % PROG_SIG_LEN] = page;
	g_uMemPagesWrittenCount++;
}

void calculateProgramSig()
{
	std::string pName;
	char pSig[13] = "";
	std::string pSigStr;
	loadSignatureFile();
	for (UINT8 i = 0; i < PROG_SIG_LEN; i++)
	{
		g_PagesCRC[i] = calculateMemPageSig(i, g_MemPagesWrittenFIFO[i]);
		int ct = snprintf(pSig, sizeof(pSig), "%03d-%08X", g_MemPagesWrittenFIFO[i], g_PagesCRC[i]);
		pSigStr.assign(pSig);
		pName = g_mBootDisksSignatures[pSigStr];
		if (!pName.empty())
		{
			g_pProgramName = pName;
			g_pProgramSig = pSigStr;
			break;
		}
	}
#ifdef DEBUG_SIGS
	displaySigCalcs();
#endif
}

// Calculate the crc of a single memory page
static UINT calculateMemPageSig(UINT crc, UINT8 pageNumber)
{
	return crc32(crc, MemGetMainPtr(pageNumber * 256), 256);
}

void displaySigCalcs()	// for debugging
{
	std::string sigsMsg = "Signatures:\n";
	char buf[50];
	UINT8 len;
	for (UINT8 i = 0; i < PROG_SIG_LEN; i++)
	{
		len = sprintf(buf, "%03d-%08X: %s\n", g_MemPagesWrittenFIFO[i], g_PagesCRC[i], "Unknown Program");
		sigsMsg.append(buf, len);
	}
	LogOutput("%s\n", sigsMsg.c_str());
}

void loadSignatureFile()
{

	MemoryTextFile_t vSigFile = MemoryTextFile_t();
	if (vSigFile.Read(g_sProgramDir + std::string("Boot Disks Signatures.yaml")))
	{
		bool bCleared = g_mBootDisksSignatures.empty();
		if (!bCleared)
		{
			LogOutput("Signatures.cpp: Boot Disks Signatures map could not be emptied\n");
		}
		std::string key;
		std::string value;
		UINT numLines = vSigFile.GetNumLines();
		UINT trailingSpaces = 0;
		char* s;
		for (UINT i = 0; i < numLines; i++)
		{
			key.clear(); value.clear();
			trailingSpaces = 0;
			s = vSigFile.GetLine(i);
			// parse the key
			while (*s && *s != ':') {
				if (*s == '#')	// if there's a comment already, pass
				{
					*s = '\0';
					break;
				}
				key.push_back(*s);
				++s;
			}
			if (!*s) continue; // if end of the string, pass
			s++; // pass the ':'
			// delimiter between key and value should be ':'
			// but it allows spaces and tabs after the ':'
			while (*s && ((*s == ' ') || (*s == '\t'))) {
				s++;
			}
			// parse the value
			// Allow for comments # after the value
			// Get rid of trailing spaces as well, between value and comment or EOL
			// But keep them if they're within the disk name
			while (*s && *s != '#') {
				if ((*s == ' ') || (*s == '\t'))
				{
					trailingSpaces++;
				}
				else {
					for (UINT j = 0; j < trailingSpaces; j++)
					{
						value.push_back(' ');
					}
					trailingSpaces = 0;
					value.push_back(*s);
				}
				++s;
			}
			g_mBootDisksSignatures[key] = value;
		}
	}
	else {
		MessageBox(g_hFrameWindow,
			TEXT("Can't read Boot Disks Signatures file"),
			TEXT("Load Boot Disks Signatures"),
			MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}


}

