/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Disk
 *
 * Author: Various
 *
 * In comments, UTAIIe is an abbreviation for a reference to "Understanding the Apple //e" by James Sather
 */

#include "StdAfx.h"

#include "SaveState_Structs_v1.h"

#include "Applewin.h"
#include "CPU.h"
#include "Disk.h"
#include "DiskLog.h"
#include "DiskFormatTrack.h"
#include "DiskImage.h"
#include "Frame.h"
#include "Log.h"
#include "Memory.h"
#include "Registry.h"
#include "Video.h"
#include "YamlHelper.h"

#include "../resource/resource.h"

#if LOG_DISK_NIBBLES_USE_RUNTIME_VAR
static bool g_bLogDisk_NibblesRW = false;	// From VS Debugger, change this to true/false during runtime for precise nibble logging
#endif

// Private ________________________________________________________________________________________

struct Drive_t
{
	int		phase;
	int		track;
	DWORD	spinning;
	DWORD	writelight;
	Disk_t	disk;

	Drive_t()
	{
		clear();
	}

	void clear()
	{
		phase = 0;
		track = 0;
		spinning = 0;
		writelight = 0;
		disk.clear();
	}
};

static WORD		currdrive       = 0;
static Drive_t	g_aFloppyDrive[NUM_DRIVES];
static BYTE		floppylatch     = 0;
static BOOL		floppymotoron   = 0;
static BOOL		floppyloadmode  = 0; // for efficiency this is not used; it's extremely unlikely to affect emulation (nickw)
static BOOL		floppywritemode = 0;
static WORD		phases = 0;					// state bits for stepper magnet phases 0 - 3
static bool		g_bSaveDiskImage = true;	// Save the DiskImage name to Registry
static UINT		g_uSlot = 0;
static unsigned __int64 g_uDiskLastCycle = 0;
static unsigned __int64 g_uDiskLastReadLatchCycle = 0;
static FormatTrack g_formatTrack;

static bool IsDriveValid( const int iDrive );
static LPCTSTR DiskGetFullPathName(const int iDrive);

#define SPINNING_CYCLES (20000*64)		// 1280000 cycles = 1.25s
#define WRITELIGHT_CYCLES (20000*64)	// 1280000 cycles = 1.25s

static bool enhancedisk = true;

//===========================================================================

bool Disk_GetEnhanceDisk(void) { return enhancedisk; }
void Disk_SetEnhanceDisk(bool bEnhanceDisk) { enhancedisk = bEnhanceDisk; }

int DiskGetCurrentDrive(void)  { return currdrive; }
int DiskGetCurrentTrack(void)  { return g_aFloppyDrive[currdrive].track; }
int DiskGetCurrentPhase(void)  { return g_aFloppyDrive[currdrive].phase; }
int DiskGetCurrentOffset(void) { return g_aFloppyDrive[currdrive].disk.byte; }
int DiskGetTrack( int drive )  { return g_aFloppyDrive[ drive   ].track; }

const char* DiskGetDiskPathFilename(const int iDrive)
{
	return g_aFloppyDrive[iDrive].disk.fullname;
}

const char* DiskGetCurrentState(void)
{
	if (g_aFloppyDrive[currdrive].disk.imagehandle == NULL)
		return "Empty";

	if (!floppymotoron)
	{
		if (g_aFloppyDrive[currdrive].spinning > 0)
			return "Off (spinning)";
		else
			return "Off";
	}
	else if (floppywritemode)
	{
		if (g_aFloppyDrive[currdrive].disk.bWriteProtected)
			return "Writing (write protected)";
		else
			return "Writing";
	}
	else
	{
		/*if (floppyloadmode)
		{
			if (g_aFloppyDrive[currdrive].disk.bWriteProtected)
				return "Reading write protect state (write protected)";
			else
				return "Reading write protect state (not write protected)";
		}
		else*/
			return "Reading";
	}
}

//===========================================================================

void Disk_LoadLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == DRIVE_1 || iDrive == DRIVE_2);

	char sFilePath[ MAX_PATH + 1];
	sFilePath[0] = 0;

	const char *pRegKey = (iDrive == DRIVE_1)
		? REGVALUE_PREF_LAST_DISK_1
		: REGVALUE_PREF_LAST_DISK_2;

	if (RegLoadString(TEXT(REG_PREFS), pRegKey, 1, sFilePath, MAX_PATH))
	{
		sFilePath[ MAX_PATH ] = 0;

		g_bSaveDiskImage = false;
		// Pass in ptr to local copy of filepath, since RemoveDisk() sets DiskPathFilename = ""
		DiskInsert(iDrive, sFilePath, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
		g_bSaveDiskImage = true;
	}
}

//===========================================================================

void Disk_SaveLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == DRIVE_1 || iDrive == DRIVE_2);

	if (!g_bSaveDiskImage)
		return;

	const char *pFileName = g_aFloppyDrive[iDrive].disk.fullname;

	if (iDrive == DRIVE_1)
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_DISK_1, TRUE, pFileName);
	else
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_DISK_2, TRUE, pFileName);

	//

	char szPathName[MAX_PATH];
	strcpy(szPathName, DiskGetFullPathName(iDrive));
	if (_tcsrchr(szPathName, TEXT('\\')))
	{
		char* pPathEnd = _tcsrchr(szPathName, TEXT('\\'))+1;
		*pPathEnd = 0;
		RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, szPathName);
	}
}

//===========================================================================

// Called by DiskControlMotor() & DiskEnable()
static void CheckSpinning(const ULONG nExecutedCycles)
{
	DWORD modechange = (floppymotoron && !g_aFloppyDrive[currdrive].spinning);

	if (floppymotoron)
		g_aFloppyDrive[currdrive].spinning = SPINNING_CYCLES;

	if (modechange)
		FrameDrawDiskLEDS( (HDC)0 );

	if (modechange)
	{
		// Set g_uDiskLastCycle when motor changes: not spinning (ie. off for 1 sec) -> on
		CpuCalcCycles(nExecutedCycles);
		g_uDiskLastCycle = g_nCumulativeCycles;
	}
}

//===========================================================================

static Disk_Status_e GetDriveLightStatus(const int iDrive)
{
	if (IsDriveValid( iDrive ))
	{
		Drive_t* pDrive = &g_aFloppyDrive[ iDrive ];

		if (pDrive->spinning)
		{
			if (pDrive->disk.bWriteProtected)
				return DISK_STATUS_PROT;

			if (pDrive->writelight)
				return DISK_STATUS_WRITE;
			else
				return DISK_STATUS_READ;
		}
		else
		{
			return DISK_STATUS_OFF;
		}
	}

	return DISK_STATUS_OFF;
}

//===========================================================================

static bool IsDriveValid(const int iDrive)
{
	return (iDrive >= 0 && iDrive < NUM_DRIVES);
}

//===========================================================================

static void AllocTrack(const int iDrive)
{
	Disk_t* pFloppy = &g_aFloppyDrive[iDrive].disk;
	pFloppy->trackimage = (LPBYTE)VirtualAlloc(NULL, NIBBLES_PER_TRACK, MEM_COMMIT, PAGE_READWRITE);
}

//===========================================================================

static void ReadTrack(const int iDrive)
{
	if (! IsDriveValid( iDrive ))
		return;

	Drive_t* pDrive = &g_aFloppyDrive[ iDrive ];
	Disk_t* pFloppy = &pDrive->disk;

	if (pDrive->track >= ImageGetNumTracks(pFloppy->imagehandle))
	{
		pFloppy->trackimagedata = false;
		return;
	}

	if (!pFloppy->trackimage)
		AllocTrack( iDrive );

	if (pFloppy->trackimage && pFloppy->imagehandle)
	{
#if LOG_DISK_TRACKS
		LOG_DISK("track $%02X%s read\r\n", pDrive->track, (pDrive->phase & 1) ? ".5" : "  ");
#endif
		ImageReadTrack(
			pFloppy->imagehandle,
			pDrive->track,
			pDrive->phase,
			pFloppy->trackimage,
			&pFloppy->nibbles);

		pFloppy->byte           = 0;
		pFloppy->trackimagedata = (pFloppy->nibbles != 0);
	}
}

//===========================================================================

static void RemoveDisk(const int iDrive)
{
	Disk_t* pFloppy = &g_aFloppyDrive[iDrive].disk;

	if (pFloppy->imagehandle)
	{
		DiskFlushCurrentTrack(iDrive);

		ImageClose(pFloppy->imagehandle);
		pFloppy->imagehandle = NULL;
	}

	if (pFloppy->trackimage)
	{
		VirtualFree(pFloppy->trackimage, 0, MEM_RELEASE);
		pFloppy->trackimage     = NULL;
		pFloppy->trackimagedata = false;
	}

	memset( pFloppy->imagename, 0, MAX_DISK_IMAGE_NAME+1 );
	memset( pFloppy->fullname , 0, MAX_DISK_FULL_NAME +1 );
	pFloppy->strFilenameInZip = "";

	Disk_SaveLastDiskImage( iDrive );
	Video_ResetScreenshotCounter( NULL );
}

//===========================================================================

static void WriteTrack(const int iDrive)
{
	Drive_t* pDrive = &g_aFloppyDrive[ iDrive ];
	Disk_t* pFloppy = &pDrive->disk;

	if (pDrive->track >= ImageGetNumTracks(pFloppy->imagehandle))
		return;

	if (pFloppy->bWriteProtected)
		return;

	if (pFloppy->trackimage && pFloppy->imagehandle)
	{
#if LOG_DISK_TRACKS
		LOG_DISK("track $%02X%s write\r\n", pDrive->track, (pDrive->phase & 0) ? ".5" : "  "); // TODO: hard-coded to whole tracks - see below (nickw)
#endif
		ImageWriteTrack(
			pFloppy->imagehandle,
			pDrive->track,
			pDrive->phase, // TODO: this should never be used; it's the current phase (half-track), not that of the track to be written (nickw)
			pFloppy->trackimage,
			pFloppy->nibbles);
	}

	pFloppy->trackimagedirty = false;
}

void DiskFlushCurrentTrack(const int iDrive)
{
	Disk_t* pFloppy = &g_aFloppyDrive[iDrive].disk;

	if (pFloppy->trackimage && pFloppy->trackimagedirty)
		WriteTrack(iDrive);
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void DiskBoot(void)
{
	// THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
	// IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
	if (g_aFloppyDrive[0].disk.imagehandle && ImageBoot(g_aFloppyDrive[0].disk.imagehandle))
		floppymotoron = 0;
}

//===========================================================================

static void __stdcall DiskControlMotor(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
	BOOL newState = address & 1;

	if (newState != floppymotoron)	// motor changed state
		g_formatTrack.DriveNotWritingTrack();

	floppymotoron = newState;
	// NB. Motor off doesn't reset the Command Decoder like reset. (UTAIIe figures 9.7 & 9.8 chip C2)
	// - so it doesn't reset this state: floppyloadmode, floppywritemode, phases
#if LOG_DISK_MOTOR
	LOG_DISK("motor %s\r\n", (floppymotoron) ? "on" : "off");
#endif
	CheckSpinning(uExecutedCycles);
}

//===========================================================================

static void __stdcall DiskControlStepper(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
	Drive_t* pDrive = &g_aFloppyDrive[currdrive];
	Disk_t* pFloppy = &pDrive->disk;

	if (!floppymotoron)	// GH#525
	{
		if (!pDrive->spinning)
		{
#if LOG_DISK_PHASES
			LOG_DISK("stepper accessed whilst motor is off and not spinning\r\n");
#endif
			return;
		}

#if LOG_DISK_PHASES
		LOG_DISK("stepper accessed whilst motor is off, but still spinning\r\n");
#endif
	}

	int phase = (address >> 1) & 3;
	int phase_bit = (1 << phase);

#if 1
	// update the magnet states
	if (address & 1)
	{
		// phase on
		phases |= phase_bit;
	}
	else
	{
		// phase off
		phases &= ~phase_bit;
	}

	// check for any stepping effect from a magnet
	// - move only when the magnet opposite the cog is off
	// - move in the direction of an adjacent magnet if one is on
	// - do not move if both adjacent magnets are on
	// momentum and timing are not accounted for ... maybe one day!
	int direction = 0;
	if (phases & (1 << ((pDrive->phase + 1) & 3)))
		direction += 1;
	if (phases & (1 << ((pDrive->phase + 3) & 3)))
		direction -= 1;

	// apply magnet step, if any
	if (direction)
	{
		pDrive->phase = MAX(0, MIN(79, pDrive->phase + direction));
		const int nNumTracksInImage = ImageGetNumTracks(pFloppy->imagehandle);
		const int newtrack = (nNumTracksInImage == 0)	? 0
														: MIN(nNumTracksInImage-1, pDrive->phase >> 1); // (round half tracks down)
		if (newtrack != pDrive->track)
		{
			DiskFlushCurrentTrack(currdrive);
			pDrive->track = newtrack;
			pFloppy->trackimagedata = false;

			g_formatTrack.DriveNotWritingTrack();
		}

		// Feature Request #201 Show track status
		// https://github.com/AppleWin/AppleWin/issues/201
		FrameDrawDiskStatus( (HDC)0 );
	}
#else
	// substitute alternate stepping code here to test
#endif

#if LOG_DISK_PHASES
	LOG_DISK("track $%02X%s phases %d%d%d%d phase %d %s address $%4X\r\n",
		pDrive->phase >> 1,
		(pDrive->phase & 1) ? ".5" : "  ",
		(phases >> 3) & 1,
		(phases >> 2) & 1,
		(phases >> 1) & 1,
		(phases >> 0) & 1,
		phase,
		(address & 1) ? "on " : "off",
		address);
#endif
}

//===========================================================================

void DiskDestroy(void)
{
	g_bSaveDiskImage = false;
	RemoveDisk(DRIVE_1);

	g_bSaveDiskImage = false;
	RemoveDisk(DRIVE_2);

	g_bSaveDiskImage = true;
}

//===========================================================================

static void __stdcall DiskEnable(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
	currdrive = address & 1;
#if LOG_DISK_ENABLE_DRIVE
	LOG_DISK("enable drive: %d\r\n", currdrive);
#endif
	g_aFloppyDrive[!currdrive].spinning   = 0;
	g_aFloppyDrive[!currdrive].writelight = 0;
	CheckSpinning(uExecutedCycles);
}

//===========================================================================

void DiskEject(const int iDrive)
{
	if (IsDriveValid(iDrive))
	{
		RemoveDisk(iDrive);
	}
}

//===========================================================================

// Return the file or zip name
// . Used by Property Sheet Page (Disk)
LPCTSTR DiskGetFullName(const int iDrive)
{
	return g_aFloppyDrive[iDrive].disk.fullname;
}

// Return the filename
// . Used by Drive Buttons' tooltips
LPCTSTR DiskGetFullDiskFilename(const int iDrive)
{
	if (!g_aFloppyDrive[iDrive].disk.strFilenameInZip.empty())
		return g_aFloppyDrive[iDrive].disk.strFilenameInZip.c_str();

	return DiskGetFullName(iDrive);
}

static LPCTSTR DiskGetFullPathName(const int iDrive)
{
	return ImageGetPathname(g_aFloppyDrive[iDrive].disk.imagehandle);
}

// Return the imagename
// . Used by Drive Button's icons & Property Sheet Page (Save snapshot)
LPCTSTR DiskGetBaseName(const int iDrive)
{
	return g_aFloppyDrive[iDrive].disk.imagename;
}
//===========================================================================

void DiskGetLightStatus(Disk_Status_e *pDisk1Status, Disk_Status_e *pDisk2Status)
{
	if (pDisk1Status)
		*pDisk1Status = GetDriveLightStatus(DRIVE_1);

	if (pDisk2Status)
		*pDisk2Status = GetDriveLightStatus(DRIVE_2);
}

//===========================================================================

void DiskInitialize(void)
{
	int loop = NUM_DRIVES;
	while (loop--)
		g_aFloppyDrive[loop].clear();
}

//===========================================================================

ImageError_e DiskInsert(const int iDrive, LPCTSTR pszImageFilename, const bool bForceWriteProtected, const bool bCreateIfNecessary)
{
	Drive_t* pDrive = &g_aFloppyDrive[iDrive];
	Disk_t* pFloppy = &pDrive->disk;

	if (pFloppy->imagehandle)
		RemoveDisk(iDrive);

	// Reset the drive's struct, but preserve the physical attributes (bug#18242: Platoon)
	// . Changing the disk (in the drive) doesn't affect the drive's head etc.
	{
		int track = pDrive->track;
		int phase = pDrive->phase;
		pDrive->clear();
		pDrive->track = track;
		pDrive->phase = phase;
	}

	const DWORD dwAttributes = GetFileAttributes(pszImageFilename);
	if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		pFloppy->bWriteProtected = false;	// Assume this is a new file to create
	else
		pFloppy->bWriteProtected = bForceWriteProtected ? true : (dwAttributes & FILE_ATTRIBUTE_READONLY);

	// Check if image is being used by the other drive, and if so remove it in order so it can be swapped
	{
		const char* pszOtherPathname = DiskGetFullPathName(!iDrive);

		char szCurrentPathname[MAX_PATH]; 
		DWORD uNameLen = GetFullPathName(pszImageFilename, MAX_PATH, szCurrentPathname, NULL);
		if (uNameLen == 0 || uNameLen >= MAX_PATH)
			strcpy_s(szCurrentPathname, MAX_PATH, pszImageFilename);

 		if (!strcmp(pszOtherPathname, szCurrentPathname))
		{
			DiskEject(!iDrive);
			FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
		}
	}

	ImageError_e Error = ImageOpen(pszImageFilename,
		&pFloppy->imagehandle,
		&pFloppy->bWriteProtected,
		bCreateIfNecessary,
		pFloppy->strFilenameInZip);

	if (Error == eIMAGE_ERROR_NONE && ImageIsMultiFileZip(pFloppy->imagehandle))
	{
		TCHAR szText[100+MAX_PATH];
		szText[sizeof(szText)-1] = 0;
		_snprintf(szText, sizeof(szText)-1, "Only the first file in a multi-file zip is supported\nUse disk image '%s' ?", pFloppy->strFilenameInZip.c_str());
		int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Multi-Zip Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
		if (nRes == IDNO)
		{
			RemoveDisk(iDrive);
			Error = eIMAGE_ERROR_REJECTED_MULTI_ZIP;
		}
	}

	if (Error == eIMAGE_ERROR_NONE)
	{
		GetImageTitle(pszImageFilename, pFloppy->imagename, pFloppy->fullname);
		Video_ResetScreenshotCounter(pFloppy->imagename);
	}
	else
	{
		Video_ResetScreenshotCounter(NULL);
	}

	Disk_SaveLastDiskImage(iDrive);
	
	return Error;
}

//===========================================================================

bool Disk_IsConditionForFullSpeed(void)
{
	return floppymotoron && enhancedisk;
}

BOOL DiskIsSpinning(void)
{
	return floppymotoron;
}

//===========================================================================

void DiskNotifyInvalidImage(const int iDrive, LPCTSTR pszImageFilename, const ImageError_e Error)
{
	TCHAR szBuffer[MAX_PATH+128];
	szBuffer[sizeof(szBuffer)-1] = 0;

	switch (Error)
	{
	case eIMAGE_ERROR_UNABLE_TO_OPEN:
	case eIMAGE_ERROR_UNABLE_TO_OPEN_GZ:
	case eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to open the file %s."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_BAD_SIZE:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("disk image is an unsupported size."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_BAD_FILE:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("OS can't access it."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("disk image format is not recognized."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED_HDV:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the file %s\n")
			TEXT("because this UniDisk 3.5/Apple IIGS/hard-disk image is not supported.\n")
			TEXT("Try inserting as a hard-disk image instead."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED_MULTI_ZIP:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("first file (%s) in this multi-zip archive is not recognized.\n")
			TEXT("Try unzipping and using the disk images directly.\n"),
			pszImageFilename,
			g_aFloppyDrive[iDrive].disk.strFilenameInZip.c_str());
		break;

	case eIMAGE_ERROR_GZ:
	case eIMAGE_ERROR_ZIP:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to use the compressed file %s\nbecause the ")
			TEXT("compressed disk image is corrupt/unsupported."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_FAILED_TO_GET_PATHNAME:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unable to GetFullPathName() for the file: %s."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_ZEROLENGTH_WRITEPROTECTED:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Unsupported zero-length write-protected file: %s."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_FAILED_TO_INIT_ZEROLENGTH:
		_snprintf(
			szBuffer,
			sizeof(szBuffer)-1,
			TEXT("Failed to resize the zero-length file: %s."),
			pszImageFilename);
		break;

	default:
		// IGNORE OTHER ERRORS SILENTLY
		return;
	}

	MessageBox(
		g_hFrameWindow,
		szBuffer,
		g_pAppTitle,
		MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}


//===========================================================================

bool DiskGetProtect(const int iDrive)
{
	if (IsDriveValid(iDrive))
	{
		if (g_aFloppyDrive[iDrive].disk.bWriteProtected)
			return true;
	}

	return false;
}


//===========================================================================

void DiskSetProtect(const int iDrive, const bool bWriteProtect)
{
	if (IsDriveValid( iDrive ))
	{
		g_aFloppyDrive[iDrive].disk.bWriteProtected = bWriteProtect;
	}
}


//===========================================================================

bool Disk_ImageIsWriteProtected(const int iDrive)
{
	if (!IsDriveValid(iDrive))
		return true;

	return ImageIsWriteProtected(g_aFloppyDrive[iDrive].disk.imagehandle);
}

//===========================================================================

bool Disk_IsDriveEmpty(const int iDrive)
{
	if (!IsDriveValid(iDrive))
		return true;

	return g_aFloppyDrive[iDrive].disk.imagehandle == NULL;
}

//===========================================================================

#if LOG_DISK_NIBBLES_WRITE
static UINT64 g_uWriteLastCycle = 0;
static UINT g_uSyncFFCount = 0;

static bool LogWriteCheckSyncFF(ULONG& uCycleDelta)
{
	bool bIsSyncFF = false;

	if (g_uWriteLastCycle == 0)	// Reset to 0 when write mode is enabled
	{
		uCycleDelta = 0;
		if (floppylatch == 0xFF)
		{
			g_uSyncFFCount = 0;
			bIsSyncFF = true;
		}
	}
	else
	{
		uCycleDelta = (ULONG) (g_nCumulativeCycles - g_uWriteLastCycle);
		if (floppylatch == 0xFF && uCycleDelta > 32)
		{
			g_uSyncFFCount++;
			bIsSyncFF = true;
		}
	}

	g_uWriteLastCycle = g_nCumulativeCycles;
	return bIsSyncFF;
}
#endif

//===========================================================================

static void __stdcall DiskReadWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	/* floppyloadmode = 0; */
	Drive_t* pDrive = &g_aFloppyDrive[currdrive];
	Disk_t* pFloppy = &pDrive->disk;

	if (!pFloppy->trackimagedata && pFloppy->imagehandle)
		ReadTrack(currdrive);

	if (!pFloppy->trackimagedata)
	{
		floppylatch = 0xFF;
		return;
	}

	// Improve precision of "authentic" drive mode - GH#125
	UINT uSpinNibbleCount = 0;
	CpuCalcCycles(nExecutedCycles);	// g_nCumulativeCycles required for uSpinNibbleCount & LogWriteCheckSyncFF()

	if (!enhancedisk && pDrive->spinning)
	{
		const ULONG nCycleDiff = (ULONG) (g_nCumulativeCycles - g_uDiskLastCycle);
		g_uDiskLastCycle = g_nCumulativeCycles;

		if (nCycleDiff > 40)
		{
			// 40 cycles for a write of a 10-bit 0xFF sync byte
			uSpinNibbleCount = nCycleDiff >> 5;	// ...but divide by 32 (not 40)

			ULONG uWrapOffset = uSpinNibbleCount % pFloppy->nibbles;
			pFloppy->byte += uWrapOffset;
			if (pFloppy->byte >= pFloppy->nibbles)
				pFloppy->byte -= pFloppy->nibbles;

#if LOG_DISK_NIBBLES_SPIN
			UINT uCompleteRevolutions = uSpinNibbleCount / pFloppy->nibbles;
			LOG_DISK("spin: revs=%d, nibbles=%d\r\n", uCompleteRevolutions, uWrapOffset);
#endif
		}
	}

	// Should really test for drive off - after 1 second drive off delay (UTAIIe page 9-13)
	// but Sherwood Forest sets shift mode and reads with the drive off, so don't check for now
	if (!floppywritemode)
	{
		const ULONG nReadCycleDiff = (ULONG) (g_nCumulativeCycles - g_uDiskLastReadLatchCycle);

		// Support partial read if disk reads are very close: (GH#582)
		// . 6 cycles (1st->2nd read) for DOS 3.3 / $BD34: "read with delays to see if disk is spinning." (Beneath Apple DOS)
		// . 6 cycles (1st->2nd read) for Curse of the Azure Bonds (loop to see if disk is spinning)
		// . 31 cycles is the max for a partial 8-bit nibble
		const ULONG kReadAccessThreshold = enhancedisk ? 6 : 31;

		if (nReadCycleDiff <= kReadAccessThreshold)
		{
			UINT invalidBits = 8 - (nReadCycleDiff / 4);	// 4 cycles per bit-cell
			floppylatch = *(pFloppy->trackimage + pFloppy->byte) >> invalidBits;
			return;	// Early return so don't update: g_uDiskLastReadLatchCycle & pFloppy->byte
		}

		floppylatch = *(pFloppy->trackimage + pFloppy->byte);
		g_uDiskLastReadLatchCycle = g_nCumulativeCycles;

#if LOG_DISK_NIBBLES_READ
  #if LOG_DISK_NIBBLES_USE_RUNTIME_VAR
		if (g_bLogDisk_NibblesRW)
  #endif
		{
			LOG_DISK("read %04X = %02X\r\n", pFloppy->byte, floppylatch);
		}

		g_formatTrack.DecodeLatchNibbleRead(floppylatch);
#endif
	}
	else if (!pFloppy->bWriteProtected) // && floppywritemode
	{
		*(pFloppy->trackimage + pFloppy->byte) = floppylatch;
		pFloppy->trackimagedirty = true;

		bool bIsSyncFF = false;
#if LOG_DISK_NIBBLES_WRITE
		ULONG uCycleDelta = 0;
		bIsSyncFF = LogWriteCheckSyncFF(uCycleDelta);
#endif

		g_formatTrack.DecodeLatchNibbleWrite(floppylatch, uSpinNibbleCount, pFloppy, bIsSyncFF);	// GH#125

#if LOG_DISK_NIBBLES_WRITE
  #if LOG_DISK_NIBBLES_USE_RUNTIME_VAR
		if (g_bLogDisk_NibblesRW)
  #endif
		{
			if (!bIsSyncFF)
				LOG_DISK("write %04X = %02X (cy=+%d)\r\n", pFloppy->byte, floppylatch, uCycleDelta);
			else
				LOG_DISK("write %04X = %02X (cy=+%d) sync #%d\r\n", pFloppy->byte, floppylatch, uCycleDelta, g_uSyncFFCount);
		}
#endif
	}

	if (++pFloppy->byte >= pFloppy->nibbles)
		pFloppy->byte = 0;

	// Feature Request #201 Show track status
	// https://github.com/AppleWin/AppleWin/issues/201
	// NB. Prevent flooding of forcing UI to redraw!!!
	if( ((pFloppy->byte) & 0xFF) == 0 )
		FrameDrawDiskStatus( (HDC)0 );
}

//===========================================================================

void DiskReset(const bool bIsPowerCycle/*=false*/)
{
	// RESET forces all switches off (UTAIIe Table 9.1)
	currdrive = 0;
	floppymotoron = 0;
	floppyloadmode = 0;
	floppywritemode = 0;
	phases = 0;

	g_formatTrack.Reset();

	if (bIsPowerCycle)	// GH#460
	{
		g_aFloppyDrive[DRIVE_1].spinning   = 0;
		g_aFloppyDrive[DRIVE_1].writelight = 0;
		g_aFloppyDrive[DRIVE_2].spinning   = 0;
		g_aFloppyDrive[DRIVE_2].writelight = 0;

		FrameRefreshStatus(DRAW_LEDS, false);
	}
}

//===========================================================================

static bool DiskSelectImage(const int iDrive, LPCSTR pszFilename)
{
	TCHAR directory[MAX_PATH] = TEXT("");
	TCHAR filename[MAX_PATH]  = TEXT("");
	TCHAR title[40];

	strcpy(filename, pszFilename);

	RegLoadString(TEXT(REG_PREFS), REGVALUE_PREF_START_DIR, 1, directory, MAX_PATH);
	_tcscpy(title, TEXT("Select Disk Image For Drive "));
	_tcscat(title, iDrive ? TEXT("2") : TEXT("1"));

	_ASSERT(sizeof(OPENFILENAME) == sizeof(OPENFILENAME_NT4));	// Required for Win98/ME support (selected by _WIN32_WINNT=0x0400 in stdafx.h)

	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = g_hFrameWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     = TEXT("All Images\0*.bin;*.do;*.dsk;*.nib;*.po;*.gz;*.zip;*.2mg;*.2img;*.iie;*.apl\0")
						  TEXT("Disk Images (*.bin,*.do,*.dsk,*.nib,*.po,*.gz,*.zip,*.2mg,*.2img,*.iie)\0*.bin;*.do;*.dsk;*.nib;*.po;*.gz;*.zip;*.2mg;*.2img;*.iie\0")
						  TEXT("All Files\0*.*\0");
	ofn.lpstrFile       = filename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = directory;
	ofn.Flags           = OFN_PATHMUSTEXIST;
	ofn.lpstrTitle      = title;

	bool bRes = false;

	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			_tcscat(filename,TEXT(".dsk"));

		ImageError_e Error = DiskInsert(iDrive, filename, ofn.Flags & OFN_READONLY, IMAGE_CREATE);
		if (Error == eIMAGE_ERROR_NONE)
		{
			bRes = true;
		}
		else
		{
			DiskNotifyInvalidImage(iDrive, filename, Error);
		}
	}

	return bRes;
}

//===========================================================================

bool DiskSelect(const int iDrive)
{
	return DiskSelectImage(iDrive, TEXT(""));
}

//===========================================================================

static void __stdcall DiskLoadWriteProtect(WORD, WORD, BYTE write, BYTE value, ULONG)
{
	/* floppyloadmode = 1; */
	if (!write)
	{
		// Notes:
		// . Should really test for drive off - after 1 second drive off delay (UTAIIe page 9-13)
		//   but Gemstone Warrior sets load mode with the drive off, so don't check for now
		// . Phase 1 on also forces write protect in the Disk II drive (UTAIIe page 9-7) but we don't implement that
		// . write mode doesn't prevent reading write protect (GH#537):
		//   "If for some reason the above write protect check were entered with the READ/WRITE switch in WRITE, 
		//    the write protect switch would still be read correctly" (UTAIIe page 9-21)
		if (g_aFloppyDrive[currdrive].disk.bWriteProtected)
			floppylatch |= 0x80;
		else
			floppylatch &= 0x7F;
	}
}

//===========================================================================

static void __stdcall DiskSetReadMode(WORD, WORD, BYTE, BYTE, ULONG)
{
	floppywritemode = 0;

	g_formatTrack.DriveSwitchedToReadMode(&g_aFloppyDrive[currdrive].disk);

#if LOG_DISK_RW_MODE
	LOG_DISK("rw mode: read\r\n");
#endif
}

//===========================================================================

static void __stdcall DiskSetWriteMode(WORD, WORD, BYTE, BYTE, ULONG uExecutedCycles)
{
	floppywritemode = 1;

	g_formatTrack.DriveSwitchedToWriteMode(g_aFloppyDrive[currdrive].disk.byte);

	BOOL modechange = !g_aFloppyDrive[currdrive].writelight;
#if LOG_DISK_RW_MODE
	LOG_DISK("rw mode: write (mode changed=%d)\r\n", modechange ? 1 : 0);
#endif
#if LOG_DISK_NIBBLES_WRITE
	g_uWriteLastCycle = 0;
#endif

	g_aFloppyDrive[currdrive].writelight = WRITELIGHT_CYCLES;

	if (modechange)
		FrameDrawDiskLEDS( (HDC)0 );
}

//===========================================================================

void DiskUpdateDriveState(DWORD cycles)
{
	int loop = NUM_DRIVES;
	while (loop--)
	{
		Drive_t* pDrive = &g_aFloppyDrive[loop];

		if (pDrive->spinning && !floppymotoron)
		{
			if (!(pDrive->spinning -= MIN(pDrive->spinning, cycles)))
			{
				FrameDrawDiskLEDS( (HDC)0 );
				FrameDrawDiskStatus( (HDC)0 );
			}
		}

		if (floppywritemode && (currdrive == loop) && pDrive->spinning)
		{
			pDrive->writelight = WRITELIGHT_CYCLES;
		}
		else if (pDrive->writelight)
		{
			if (!(pDrive->writelight -= MIN(pDrive->writelight, cycles)))
			{
				FrameDrawDiskLEDS( (HDC)0 );
				FrameDrawDiskStatus( (HDC)0 );
			}
		}
	}
}

//===========================================================================

bool DiskDriveSwap(void)
{
	// Refuse to swap if either Disk][ is active
	// TODO: if Shift-Click then FORCE drive swap to bypass message
	if (g_aFloppyDrive[DRIVE_1].spinning || g_aFloppyDrive[DRIVE_2].spinning)
	{
		// 1.26.2.4 Prompt when trying to swap disks while drive is on instead of silently failing
		int status = MessageBox(
			g_hFrameWindow,
			"WARNING:\n"
				"\n"
				"\tAttempting to swap a disk while a drive is on\n"
				"\t\t--> is NOT recommended <--\n"
				"\tas this will most likely read/write incorrect data!\n"
				"\n"
				"If the other drive is empty then swapping is harmless. The"
				" computer will appear to 'hang' trying to read non-existent data but"
				" you can safely swap disks once more to restore the original disk.\n"
				"\n"
				"Do you still wish to swap disks?",
			"Trying to swap a disk while a drive is on ...",
			MB_ICONWARNING | MB_YESNOCANCEL
		);

		switch( status )
		{
			case IDNO:
			case IDCANCEL:
				return false;
			default:
				break; // User is OK with swapping disks so let them proceed at their own risk
		}
	}

	DiskFlushCurrentTrack(DRIVE_1);
	DiskFlushCurrentTrack(DRIVE_2);

	// Swap disks between drives
	// . NB. We swap trackimage ptrs (so don't need to swap the buffers' data)
	std::swap(g_aFloppyDrive[DRIVE_1].disk, g_aFloppyDrive[DRIVE_2].disk);

	// Invalidate the trackimage so that a read latch will re-read the track for the new floppy (GH#543)
	g_aFloppyDrive[DRIVE_1].disk.trackimagedata = false;
	g_aFloppyDrive[DRIVE_2].disk.trackimagedata = false;

	Disk_SaveLastDiskImage(DRIVE_1);
	Disk_SaveLastDiskImage(DRIVE_2);

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES, false);

	return true;
}

//===========================================================================

static BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
static BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

// TODO: LoadRom_Disk_Floppy()
void DiskLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	const UINT DISK2_FW_SIZE = APPLE_SLOT_SIZE;

	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_DISK2_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != DISK2_FW_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	memcpy(pCxRomPeripheral + uSlot*APPLE_SLOT_SIZE, pData, DISK2_FW_SIZE);

	// Note: We used to disable the track stepping delay in the Disk II controller firmware by
	// patching $C64C with $A9,$00,$EA. Now not doing this since:
	// . Authentic Speed should be authentic
	// . Enhanced Speed runs emulation unthrottled, so removing the delay has negligible effect
	// . Patching the firmware breaks the ADC checksum used by "The CIA Files" (Tricky Dick)
	// . In this case we can patch to compensate for an ADC or EOR checksum but not both (nickw)

	RegisterIoHandler(uSlot, Disk_IORead, Disk_IOWrite, NULL, NULL, NULL, NULL);

	g_uSlot = uSlot;
}

//===========================================================================

static BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	switch (addr & 0xF)
	{
	case 0x0:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x1:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x2:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x3:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x4:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x5:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x6:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x7:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x8:	DiskControlMotor(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x9:	DiskControlMotor(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xA:	DiskEnable(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xB:	DiskEnable(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xC:	DiskReadWrite(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xD:	DiskLoadWriteProtect(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xE:	DiskSetReadMode(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xF:	DiskSetWriteMode(pc, addr, bWrite, d, nExecutedCycles); break;
	}

	// only even addresses return the latch (UTAIIe Table 9.1)
	if (!(addr & 1))
		return floppylatch;
	else
		return MemReadFloatingBus(nExecutedCycles);
}

static BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	switch (addr & 0xF)
	{
	case 0x0:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x1:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x2:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x3:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x4:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x5:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x6:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x7:	DiskControlStepper(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x8:	DiskControlMotor(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0x9:	DiskControlMotor(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xA:	DiskEnable(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xB:	DiskEnable(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xC:	DiskReadWrite(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xD:	DiskLoadWriteProtect(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xE:	DiskSetReadMode(pc, addr, bWrite, d, nExecutedCycles); break;
	case 0xF:	DiskSetWriteMode(pc, addr, bWrite, d, nExecutedCycles); break;
	}

	// any address writes the latch via sequencer LD command (74LS323 datasheet)
	if (floppywritemode /* && floppyloadmode */)
	{
		floppylatch = d;
	}
	return 0;
}

//===========================================================================

int DiskSetSnapshot_v1(const SS_CARD_DISK2* const pSS)
{
	if(pSS->Hdr.UnitHdr.hdr.v1.dwVersion > MAKE_VERSION(1,0,0,2))
		return -1;

	phases  		= pSS->phases;
	currdrive		= pSS->currdrive;
	//diskaccessed	= pSS->diskaccessed;	// deprecated
	enhancedisk		= pSS->enhancedisk ? true : false;
	floppylatch		= pSS->floppylatch;
	floppymotoron	= pSS->floppymotoron;
	floppywritemode	= pSS->floppywritemode;

	// Eject all disks first in case Drive-2 contains disk to be inserted into Drive-1
	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		DiskEject(i);	// Remove any disk & update Registry to reflect empty drive
		g_aFloppyDrive[i].clear();
	}

	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		if(pSS->Unit[i].szFileName[0] == 0x00)
			continue;

		DWORD dwAttributes = GetFileAttributes(pSS->Unit[i].szFileName);
		if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		{
			// Get user to browse for file
			DiskSelectImage(i, pSS->Unit[i].szFileName);

			dwAttributes = GetFileAttributes(pSS->Unit[i].szFileName);
		}

		bool bImageError = false;
		if(dwAttributes != INVALID_FILE_ATTRIBUTES)
		{
			if(DiskInsert(i, pSS->Unit[i].szFileName, dwAttributes & FILE_ATTRIBUTE_READONLY, IMAGE_DONT_CREATE) != eIMAGE_ERROR_NONE)
				bImageError = true;

			// DiskInsert() sets up:
			// . imagename
			// . fullname
			// . writeprotected
		}

		//

//		strcpy(g_aFloppyDrive[i].fullname, pSS->Unit[i].szFileName);
		g_aFloppyDrive[i].track				= pSS->Unit[i].track;
		g_aFloppyDrive[i].phase				= pSS->Unit[i].phase;
		g_aFloppyDrive[i].spinning			= pSS->Unit[i].spinning;
		g_aFloppyDrive[i].writelight		= pSS->Unit[i].writelight;

		g_aFloppyDrive[i].disk.byte				= pSS->Unit[i].byte;
//		g_aFloppyDrive[i].disk.writeprotected	= pSS->Unit[i].writeprotected;
		g_aFloppyDrive[i].disk.trackimagedata	= pSS->Unit[i].trackimagedata ? true : false;
		g_aFloppyDrive[i].disk.trackimagedirty	= pSS->Unit[i].trackimagedirty ? true : false;
		g_aFloppyDrive[i].disk.nibbles			= pSS->Unit[i].nibbles;

		//

		if(!bImageError)
		{
			if((g_aFloppyDrive[i].disk.trackimage == NULL) && g_aFloppyDrive[i].disk.nibbles)
				AllocTrack(i);

			if(g_aFloppyDrive[i].disk.trackimage == NULL)
				bImageError = true;
			else
				memcpy(g_aFloppyDrive[i].disk.trackimage, pSS->Unit[i].nTrack, NIBBLES_PER_TRACK);
		}

		if(bImageError)
		{
			g_aFloppyDrive[i].disk.trackimagedata	= false;
			g_aFloppyDrive[i].disk.trackimagedirty	= false;
			g_aFloppyDrive[i].disk.nibbles			= 0;
		}
	}

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	return 0;
}

//===========================================================================

// Unit version history:  
// 2: Added: Format Track state & DiskLastCycle
// 3: Added: DiskLastReadLatchCycle
static const UINT kUNIT_VERSION = 3;

#define SS_YAML_VALUE_CARD_DISK2 "Disk]["

#define SS_YAML_KEY_PHASES "Phases"
#define SS_YAML_KEY_CURRENT_DRIVE "Current Drive"
#define SS_YAML_KEY_DISK_ACCESSED "Disk Accessed"
#define SS_YAML_KEY_ENHANCE_DISK "Enhance Disk"
#define SS_YAML_KEY_FLOPPY_LATCH "Floppy Latch"
#define SS_YAML_KEY_FLOPPY_MOTOR_ON "Floppy Motor On"
#define SS_YAML_KEY_FLOPPY_WRITE_MODE "Floppy Write Mode"
#define SS_YAML_KEY_LAST_CYCLE "Last Cycle"
#define SS_YAML_KEY_LAST_READ_LATCH_CYCLE "Last Read Latch Cycle"

#define SS_YAML_KEY_DISK2UNIT "Unit"
#define SS_YAML_KEY_FILENAME "Filename"
#define SS_YAML_KEY_TRACK "Track"
#define SS_YAML_KEY_PHASE "Phase"
#define SS_YAML_KEY_BYTE "Byte"
#define SS_YAML_KEY_WRITE_PROTECTED "Write Protected"
#define SS_YAML_KEY_SPINNING "Spinning"
#define SS_YAML_KEY_WRITE_LIGHT "Write Light"
#define SS_YAML_KEY_NIBBLES "Nibbles"
#define SS_YAML_KEY_TRACK_IMAGE_DATA "Track Image Data"
#define SS_YAML_KEY_TRACK_IMAGE_DIRTY "Track Image Dirty"
#define SS_YAML_KEY_TRACK_IMAGE "Track Image"

std::string DiskGetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_DISK2);
	return name;
}

static void DiskSaveSnapshotDisk2Unit(YamlSaveHelper& yamlSaveHelper, UINT unit)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_DISK2UNIT, unit);
	yamlSaveHelper.SaveString(SS_YAML_KEY_FILENAME, g_aFloppyDrive[unit].disk.fullname);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_TRACK, g_aFloppyDrive[unit].track);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_PHASE, g_aFloppyDrive[unit].phase);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_BYTE, g_aFloppyDrive[unit].disk.byte);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_WRITE_PROTECTED, g_aFloppyDrive[unit].disk.bWriteProtected);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_SPINNING, g_aFloppyDrive[unit].spinning);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_WRITE_LIGHT, g_aFloppyDrive[unit].writelight);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_NIBBLES, g_aFloppyDrive[unit].disk.nibbles);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_TRACK_IMAGE_DATA, g_aFloppyDrive[unit].disk.trackimagedata);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_TRACK_IMAGE_DIRTY, g_aFloppyDrive[unit].disk.trackimagedirty);

	if (g_aFloppyDrive[unit].disk.trackimage)
	{
		YamlSaveHelper::Label image(yamlSaveHelper, "%s:\n", SS_YAML_KEY_TRACK_IMAGE);
		yamlSaveHelper.SaveMemory(g_aFloppyDrive[unit].disk.trackimage, NIBBLES_PER_TRACK);
	}
}

void DiskSaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, DiskGetSnapshotCardName(), g_uSlot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.SaveHexUint4(SS_YAML_KEY_PHASES, phases);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_CURRENT_DRIVE, currdrive);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_DISK_ACCESSED, false);	// deprecated
	yamlSaveHelper.SaveBool(SS_YAML_KEY_ENHANCE_DISK, enhancedisk);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_FLOPPY_LATCH, floppylatch);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_FLOPPY_MOTOR_ON, floppymotoron == TRUE);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_FLOPPY_WRITE_MODE, floppywritemode == TRUE);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_LAST_CYCLE, g_uDiskLastCycle);	// v2
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_LAST_READ_LATCH_CYCLE, g_uDiskLastReadLatchCycle);	// v3
	g_formatTrack.SaveSnapshot(yamlSaveHelper);	// v2

	DiskSaveSnapshotDisk2Unit(yamlSaveHelper, DRIVE_1);
	DiskSaveSnapshotDisk2Unit(yamlSaveHelper, DRIVE_2);
}

static void DiskLoadSnapshotDriveUnit(YamlLoadHelper& yamlLoadHelper, UINT unit)
{
	std::string disk2UnitName = std::string(SS_YAML_KEY_DISK2UNIT) + (unit == DRIVE_1 ? std::string("0") : std::string("1"));
	if (!yamlLoadHelper.GetSubMap(disk2UnitName))
		throw std::string("Card: Expected key: ") + disk2UnitName;

	bool bImageError = false;

	g_aFloppyDrive[unit].disk.fullname[0] = 0;
	g_aFloppyDrive[unit].disk.imagename[0] = 0;
	g_aFloppyDrive[unit].disk.bWriteProtected = false;	// Default to false (until image is successfully loaded below)

	std::string filename = yamlLoadHelper.LoadString(SS_YAML_KEY_FILENAME);
	if (!filename.empty())
	{
		DWORD dwAttributes = GetFileAttributes(filename.c_str());
		if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		{
			// Get user to browse for file
			DiskSelectImage(unit, filename.c_str());

			dwAttributes = GetFileAttributes(filename.c_str());
		}

		bImageError = (dwAttributes == INVALID_FILE_ATTRIBUTES);
		if (!bImageError)
		{
			if(DiskInsert(unit, filename.c_str(), dwAttributes & FILE_ATTRIBUTE_READONLY, IMAGE_DONT_CREATE) != eIMAGE_ERROR_NONE)
				bImageError = true;

			// DiskInsert() zeros g_aFloppyDrive[unit], then sets up:
			// . imagename
			// . fullname
			// . writeprotected
		}
	}

	g_aFloppyDrive[unit].track			= yamlLoadHelper.LoadUint(SS_YAML_KEY_TRACK);
	g_aFloppyDrive[unit].phase			= yamlLoadHelper.LoadUint(SS_YAML_KEY_PHASE);
	g_aFloppyDrive[unit].disk.byte		= yamlLoadHelper.LoadUint(SS_YAML_KEY_BYTE);
	yamlLoadHelper.LoadBool(SS_YAML_KEY_WRITE_PROTECTED);	// Consume
	g_aFloppyDrive[unit].spinning		= yamlLoadHelper.LoadUint(SS_YAML_KEY_SPINNING);
	g_aFloppyDrive[unit].writelight		= yamlLoadHelper.LoadUint(SS_YAML_KEY_WRITE_LIGHT);
	g_aFloppyDrive[unit].disk.nibbles			= yamlLoadHelper.LoadUint(SS_YAML_KEY_NIBBLES);
	g_aFloppyDrive[unit].disk.trackimagedata	= yamlLoadHelper.LoadUint(SS_YAML_KEY_TRACK_IMAGE_DATA) ? true : false;
	g_aFloppyDrive[unit].disk.trackimagedirty	= yamlLoadHelper.LoadUint(SS_YAML_KEY_TRACK_IMAGE_DIRTY) ? true : false;

	std::vector<BYTE> track(NIBBLES_PER_TRACK);
	if (yamlLoadHelper.GetSubMap(SS_YAML_KEY_TRACK_IMAGE))
	{
		yamlLoadHelper.LoadMemory(&track[0], NIBBLES_PER_TRACK);
		yamlLoadHelper.PopMap();
	}

	yamlLoadHelper.PopMap();

	//

	if (!filename.empty() && !bImageError)
	{
		if ((g_aFloppyDrive[unit].disk.trackimage == NULL) && g_aFloppyDrive[unit].disk.nibbles)
			AllocTrack(unit);

		if (g_aFloppyDrive[unit].disk.trackimage == NULL)
			bImageError = true;
		else
			memcpy(g_aFloppyDrive[unit].disk.trackimage, &track[0], NIBBLES_PER_TRACK);
	}

	if (bImageError)
	{
		g_aFloppyDrive[unit].disk.trackimagedata	= false;
		g_aFloppyDrive[unit].disk.trackimagedirty	= false;
		g_aFloppyDrive[unit].disk.nibbles			= 0;
	}
}

bool DiskLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != 6)	// fixme
		throw std::string("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	phases  		= yamlLoadHelper.LoadUint(SS_YAML_KEY_PHASES);
	currdrive		= yamlLoadHelper.LoadUint(SS_YAML_KEY_CURRENT_DRIVE);
	(void)			  yamlLoadHelper.LoadBool(SS_YAML_KEY_DISK_ACCESSED);	// deprecated - but retrieve the value to avoid the "State: Unknown key (Disk Accessed)" warning
	enhancedisk		= yamlLoadHelper.LoadBool(SS_YAML_KEY_ENHANCE_DISK);
	floppylatch		= yamlLoadHelper.LoadUint(SS_YAML_KEY_FLOPPY_LATCH);
	floppymotoron	= yamlLoadHelper.LoadBool(SS_YAML_KEY_FLOPPY_MOTOR_ON);
	floppywritemode	= yamlLoadHelper.LoadBool(SS_YAML_KEY_FLOPPY_WRITE_MODE);

	if (version >= 2)
	{
		g_uDiskLastCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_LAST_CYCLE);
		g_formatTrack.LoadSnapshot(yamlLoadHelper);
	}

	if (version >= 3)
	{
		g_uDiskLastReadLatchCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_LAST_READ_LATCH_CYCLE);
	}

	// Eject all disks first in case Drive-2 contains disk to be inserted into Drive-1
	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		DiskEject(i);	// Remove any disk & update Registry to reflect empty drive
		g_aFloppyDrive[i].clear();
	}

	DiskLoadSnapshotDriveUnit(yamlLoadHelper, DRIVE_1);
	DiskLoadSnapshotDriveUnit(yamlLoadHelper, DRIVE_2);

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	return true;
}
