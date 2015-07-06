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
 * In comments, UTA2E is an abbreviation for a reference to "Understanding the Apple //e" by James Sather
 */

#include "StdAfx.h"

#include "AppleWin.h"
#include "Disk.h"
#include "DiskImage.h"
#include "Frame.h"
#include "Log.h"
#include "Memory.h"
#include "Registry.h"
#include "Video.h"

#include "..\resource\resource.h"

#define LOG_DISK_ENABLED 0
#define LOG_DISK_TRACKS 1
#define LOG_DISK_MOTOR 0
#define LOG_DISK_PHASES 0
#define LOG_DISK_NIBBLES 0

// __VA_ARGS__ not supported on MSVC++ .NET 7.x
#if (LOG_DISK_ENABLED)
	#if !defined(_VC71)
		#define LOG_DISK(format, ...) LOG(format, __VA_ARGS__)
	#else
		#define LOG_DISK	 LogOutput
	#endif
#else
	#if !defined(_VC71)
		#define LOG_DISK(...)
	#else
		#define LOG_DISK(x)
	#endif
#endif

// Public _________________________________________________________________________________________

	BOOL enhancedisk = 1;					// TODO: Make static & add accessor funcs

// Private ________________________________________________________________________________________

	const int MAX_DISK_IMAGE_NAME = 15;
	const int MAX_DISK_FULL_NAME  = 127;

	struct Disk_t
	{
		TCHAR  imagename[ MAX_DISK_IMAGE_NAME + 1 ];	// <FILENAME> (ie. no extension)
		TCHAR  fullname [ MAX_DISK_FULL_NAME  + 1 ];	// <FILENAME.EXT> or <FILENAME.zip>  : This is persisted to the snapshot file
		std::string strDiskPathFilename;
		std::string strFilenameInZip;					// 0x00           or <FILENAME.EXT>
		HIMAGE imagehandle;					// Init'd by DiskInsert() -> ImageOpen()
		int    track;
		LPBYTE trackimage;
		int    phase;
		int    byte;
		bool   bWriteProtected;
		BOOL   trackimagedata;
		BOOL   trackimagedirty;
		DWORD  spinning;
		DWORD  writelight;
		int    nibbles;						// Init'd by ReadTrack() -> ImageReadTrack()

		const Disk_t& operator= (const Disk_t& other)
		{
			memcpy(imagename, other.imagename, sizeof(imagename));
			memcpy(fullname , other.fullname,  sizeof(fullname));
			strDiskPathFilename = other.strDiskPathFilename;
			strFilenameInZip    = other.strFilenameInZip;
			imagehandle         = other.imagehandle;
			track               = other.track;
			trackimage          = other.trackimage;
			phase               = other.phase;
			byte                = other.byte;
			bWriteProtected     = other.bWriteProtected;
			trackimagedata      = other.trackimagedata;
			trackimagedirty     = other.trackimagedirty;
			spinning            = other.spinning;
			writelight          = other.writelight;
			nibbles             = other.nibbles;
			return *this;
		}
	};

static WORD		currdrive       = 0;
static BOOL		diskaccessed    = 0;
static Disk_t	g_aFloppyDisk[NUM_DRIVES];
static BYTE		floppylatch     = 0;
static BOOL		floppymotoron   = 0;
static BOOL		floppyloadmode  = 0; // for efficiency this is not used; it's extremely unlikely to affect emulation (nickw)
static BOOL		floppywritemode = 0;
static WORD		phases;						// state bits for stepper magnet phases 0 - 3
static bool		g_bSaveDiskImage = true;	// Save the DiskImage name to Registry

static void CheckSpinning();
static Disk_Status_e GetDriveLightStatus( const int iDrive );
static bool IsDriveValid( const int iDrive );
static void ReadTrack (int drive);
static void RemoveDisk (int drive);
static void WriteTrack (int drive);

//===========================================================================

int DiskGetCurrentDrive(void)  { return currdrive; }
int DiskGetCurrentTrack(void)  { return g_aFloppyDisk[currdrive].track; }
int DiskGetCurrentPhase(void)  { return g_aFloppyDisk[currdrive].phase; }
int DiskGetCurrentOffset(void) { return g_aFloppyDisk[currdrive].byte; }
int DiskGetTrack( int drive )  { return g_aFloppyDisk[ drive   ].track; }

const std::string& DiskGetDiskPathFilename(const int iDrive)
{
	return g_aFloppyDisk[iDrive].strDiskPathFilename;
}

static void DiskSetDiskPathFilename(const int iDrive, const std::string strPathName)
{
	g_aFloppyDisk[iDrive].strDiskPathFilename = strPathName;
}

char* DiskGetCurrentState(void)
{
	if (g_aFloppyDisk[currdrive].imagehandle == NULL)
		return "Empty";

	if (!floppymotoron)
	{
		if (g_aFloppyDisk[currdrive].spinning > 0)
			return "Off (spinning)";
		else
			return "Off";
	}
	else if (floppywritemode)
	{
		if (g_aFloppyDisk[currdrive].bWriteProtected)
			return "Writing (write protected)";
		else
			return "Writing";
	}
	else
	{
		/*if (floppyloadmode)
		{
			if (g_aFloppyDisk[currdrive].bWriteProtected)
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

	char *pRegKey = (iDrive == DRIVE_1)
		? REGVALUE_PREF_LAST_DISK_1
		: REGVALUE_PREF_LAST_DISK_2;

	if (RegLoadString(TEXT(REG_PREFS),pRegKey,1,sFilePath,MAX_PATH))
	{
		sFilePath[ MAX_PATH ] = 0;
		DiskSetDiskPathFilename(iDrive, sFilePath);

#if _DEBUG
//		MessageBox(g_hFrameWindow,pFileName,pRegKey,MB_OK);
#endif

		//	_tcscat(imagefilename,TEXT("MASTER.DSK")); // TODO: Should remember last disk by user
		g_bSaveDiskImage = false;
		// Pass in ptr to local copy of filepath, since RemoveDisk() sets DiskPathFilename = ""
		DiskInsert(iDrive, sFilePath, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
		g_bSaveDiskImage = true;
	}
	//else MessageBox(g_hFrameWindow,"Reg Key/Value not found",pRegKey,MB_OK);
}

//===========================================================================

void Disk_SaveLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == DRIVE_1 || iDrive == DRIVE_2);

	if (!g_bSaveDiskImage)
		return;

	const char *pFileName = DiskGetDiskPathFilename(iDrive).c_str();

	if (iDrive == DRIVE_1)
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_DISK_1, TRUE, pFileName);
	else
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_DISK_2, TRUE, pFileName);
}

//===========================================================================

static void CheckSpinning(void)
{
	DWORD modechange = (floppymotoron && !g_aFloppyDisk[currdrive].spinning);

	if (floppymotoron)
		g_aFloppyDisk[currdrive].spinning = 20000;

	if (modechange)
		//FrameRefreshStatus(DRAW_LEDS);
		FrameDrawDiskLEDS( (HDC)0 );
}

//===========================================================================

static Disk_Status_e GetDriveLightStatus(const int iDrive)
{
	if (IsDriveValid( iDrive ))
	{
		Disk_t *pFloppy = & g_aFloppyDisk[ iDrive ];

		if (pFloppy->spinning)
		{
			if (pFloppy->bWriteProtected)
				return DISK_STATUS_PROT;

			if (pFloppy->writelight)
				return DISK_STATUS_WRITE;
			else
				return DISK_STATUS_READ;
		}
		else
			return DISK_STATUS_OFF;
	}

	return DISK_STATUS_OFF;
}

//===========================================================================

static void GetImageTitle(LPCTSTR imagefilename, Disk_t* fptr)
{
	TCHAR   imagetitle[ MAX_DISK_FULL_NAME+1 ];
	LPCTSTR startpos = imagefilename;

	// imagetitle = <FILENAME.EXT>
	if (_tcsrchr(startpos,TEXT('\\')))
		startpos = _tcsrchr(startpos,TEXT('\\'))+1;

	_tcsncpy(imagetitle,startpos,MAX_DISK_FULL_NAME);
	imagetitle[MAX_DISK_FULL_NAME] = 0;

	// if imagetitle contains a lowercase char, then found=1 (why?)
	BOOL found = 0;
	int  loop  = 0;
	while (imagetitle[loop] && !found)
	{
		if (IsCharLower(imagetitle[loop]))
			found = 1;
		else
			loop++;
	}

	if ((!found) && (loop > 2))
		CharLowerBuff(imagetitle+1,_tcslen(imagetitle+1));

	// fptr->fullname = <FILENAME.EXT>
	_tcsncpy( fptr->fullname, imagetitle, MAX_DISK_FULL_NAME );
	fptr->fullname[ MAX_DISK_FULL_NAME ] = 0;

	if (imagetitle[0])
	{
		LPTSTR dot = imagetitle;
		if (_tcsrchr(dot,TEXT('.')))
			dot = _tcsrchr(dot,TEXT('.'));
		if (dot > imagetitle)
			*dot = 0;
	}

	// fptr->imagename = <FILENAME> (ie. no extension)
	_tcsncpy( fptr->imagename, imagetitle, MAX_DISK_IMAGE_NAME );
	fptr->imagename[ MAX_DISK_IMAGE_NAME ] = 0;
}


//===========================================================================

static bool IsDriveValid(const int iDrive)
{
	return (iDrive >= 0 && iDrive < NUM_DRIVES);
}

//===========================================================================

static void AllocTrack(const int iDrive)
{
	Disk_t * fptr = &g_aFloppyDisk[iDrive];
	fptr->trackimage = (LPBYTE)VirtualAlloc(NULL, NIBBLES_PER_TRACK, MEM_COMMIT, PAGE_READWRITE);
}

//===========================================================================

static void ReadTrack(const int iDrive)
{
	if (! IsDriveValid( iDrive ))
		return;

	Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];

	if (pFloppy->track >= ImageGetNumTracks(pFloppy->imagehandle))
	{
		pFloppy->trackimagedata = 0;
		return;
	}

	if (! pFloppy->trackimage)
		AllocTrack( iDrive );

	if (pFloppy->trackimage && pFloppy->imagehandle)
	{
#if LOG_DISK_TRACKS
		LOG_DISK("track $%02X%s read\r\n", pFloppy->track, (pFloppy->phase & 1) ? ".5" : "  ");
#endif
		ImageReadTrack(
			pFloppy->imagehandle,
			pFloppy->track,
			pFloppy->phase,
			pFloppy->trackimage,
			&pFloppy->nibbles);

		pFloppy->byte           = 0;
		pFloppy->trackimagedata = (pFloppy->nibbles != 0);
	}
}

//===========================================================================

static void RemoveDisk(const int iDrive)
{
	Disk_t *pFloppy = &g_aFloppyDisk[iDrive];

	if (pFloppy->imagehandle)
	{
		if (pFloppy->trackimage && pFloppy->trackimagedirty)
			WriteTrack( iDrive);

		ImageClose(pFloppy->imagehandle);
		pFloppy->imagehandle = (HIMAGE)0;
	}

	if (pFloppy->trackimage)
	{
		VirtualFree(pFloppy->trackimage,0,MEM_RELEASE);
		pFloppy->trackimage     = NULL;
		pFloppy->trackimagedata = 0;
	}

	memset( pFloppy->imagename, 0, MAX_DISK_IMAGE_NAME+1 );
	memset( pFloppy->fullname , 0, MAX_DISK_FULL_NAME +1 );
	pFloppy->strFilenameInZip = "";
	DiskSetDiskPathFilename(iDrive, "");

	Disk_SaveLastDiskImage( iDrive );
	Video_ResetScreenshotCounter( NULL );
}

//===========================================================================

static void WriteTrack(const int iDrive)
{
	Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];

	if (pFloppy->track >= ImageGetNumTracks(pFloppy->imagehandle))
		return;

	if (pFloppy->bWriteProtected)
		return;

	if (pFloppy->trackimage && pFloppy->imagehandle)
	{
#if LOG_DISK_TRACKS
		LOG_DISK("track $%02X%s write\r\n", pFloppy->track, (pFloppy->phase & 0) ? ".5" : "  "); // TODO: hard-coded to whole tracks - see below (nickw)
#endif
		ImageWriteTrack(
			pFloppy->imagehandle,
			pFloppy->track,
			pFloppy->phase, // TODO: this should never be used; it's the current phase (half-track), not that of the track to be written (nickw)
			pFloppy->trackimage,
			pFloppy->nibbles);
	}

	pFloppy->trackimagedirty = 0;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void DiskBoot(void)
{
	// THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
	// IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
	if (g_aFloppyDisk[0].imagehandle && ImageBoot(g_aFloppyDisk[0].imagehandle))
		floppymotoron = 0;
}

//===========================================================================

static void __stdcall DiskControlMotor(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
	floppymotoron = address & 1;
#if LOG_DISK_MOTOR
	LOG_DISK("motor %s\r\n", (floppymotoron) ? "on" : "off");
#endif
	CheckSpinning();
}

//===========================================================================

static void __stdcall DiskControlStepper(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles)
{
	Disk_t * fptr = &g_aFloppyDisk[currdrive];
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
	if (phases & (1 << ((fptr->phase + 1) & 3)))
		direction += 1;
	if (phases & (1 << ((fptr->phase + 3) & 3)))
		direction -= 1;

	// apply magnet step, if any
	if (direction)
	{
		fptr->phase = MAX(0, MIN(79, fptr->phase + direction));
		const int nNumTracksInImage = ImageGetNumTracks(fptr->imagehandle);
		const int newtrack = (nNumTracksInImage == 0)	? 0
														: MIN(nNumTracksInImage-1, fptr->phase >> 1); // (round half tracks down)
		if (newtrack != fptr->track)
		{
			if (fptr->trackimage && fptr->trackimagedirty)
			{
				WriteTrack(currdrive);
			}
			fptr->track          = newtrack;
			fptr->trackimagedata = 0;
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
		fptr->phase >> 1,
		(fptr->phase & 1) ? ".5" : "  ",
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
	g_aFloppyDisk[!currdrive].spinning   = 0;
	g_aFloppyDisk[!currdrive].writelight = 0;
	CheckSpinning();
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
	return g_aFloppyDisk[iDrive].fullname;
}

// Return the filename
// . Used by Drive Buttons' tooltips
LPCTSTR DiskGetFullDiskFilename(const int iDrive)
{
	if (!g_aFloppyDisk[iDrive].strFilenameInZip.empty())
		return g_aFloppyDisk[iDrive].strFilenameInZip.c_str();

	return DiskGetFullName(iDrive);
}

// Return the imagename
// . Used by Drive Button's icons & Property Sheet Page (Save snapshot)
LPCTSTR DiskGetBaseName(const int iDrive)
{
	return g_aFloppyDisk[iDrive].imagename;
}
//===========================================================================

void DiskGetLightStatus(Disk_Status_e *pDisk1Status_, Disk_Status_e *pDisk2Status_)
{
	if (pDisk1Status_)
		*pDisk1Status_ = GetDriveLightStatus( 0 );

	if (pDisk2Status_)
		*pDisk2Status_ = GetDriveLightStatus( 1 );
}

//===========================================================================

void DiskInitialize(void)
{
	int loop = NUM_DRIVES;
	while (loop--)
		ZeroMemory(&g_aFloppyDisk[loop],sizeof(Disk_t ));

	TCHAR imagefilename[MAX_PATH];
	_tcscpy(imagefilename,g_sProgramDir);
}

//===========================================================================

ImageError_e DiskInsert(const int iDrive, LPCTSTR pszImageFilename, const bool bForceWriteProtected, const bool bCreateIfNecessary)
{
	Disk_t * fptr = &g_aFloppyDisk[iDrive];
	if (fptr->imagehandle)
		RemoveDisk(iDrive);

	// Reset the drive's struct, but preserve the physical attributes (bug#18242: Platoon)
	// . Changing the disk (in the drive) doesn't affect the drive's head etc.
	{
		int track = fptr->track;
		int phase = fptr->phase;
		ZeroMemory(fptr, sizeof(Disk_t));
		fptr->track = track;
		fptr->phase = phase;
	}

	const DWORD dwAttributes = GetFileAttributes(pszImageFilename);
	if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		fptr->bWriteProtected = false;	// Assume this is a new file to create
	else
		fptr->bWriteProtected = bForceWriteProtected ? true : (dwAttributes & FILE_ATTRIBUTE_READONLY);

	// Check if image is being used by the other HDD, and unplug it in order to be swapped
	std::string otherDisk = DiskGetDiskPathFilename(!iDrive);
	if (!strcmp(otherDisk.c_str(), pszImageFilename)) {
		DiskEject(!iDrive);
	}

	ImageError_e Error = ImageOpen(pszImageFilename,
		&fptr->imagehandle,
		&fptr->bWriteProtected,
		bCreateIfNecessary,
		fptr->strFilenameInZip);

	if (Error == eIMAGE_ERROR_NONE && ImageIsMultiFileZip(fptr->imagehandle))
	{
		TCHAR szText[100+MAX_PATH];
		wsprintf(szText, "Only the first file in a multi-file zip is supported\nUse disk image '%s' ?", fptr->strFilenameInZip.c_str());
		int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Multi-Zip Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
		if (nRes == IDNO)
		{
			RemoveDisk(iDrive);
			Error = eIMAGE_ERROR_REJECTED_MULTI_ZIP;
		}
	}

	if (Error == eIMAGE_ERROR_NONE)
	{
		GetImageTitle(pszImageFilename, fptr);

		DiskSetDiskPathFilename(iDrive, pszImageFilename);

		//MessageBox( g_hFrameWindow, imagefilename, fptr->imagename, MB_OK );
		Video_ResetScreenshotCounter( fptr->imagename );
	}
	else
	{
		Video_ResetScreenshotCounter( NULL );
	}

	Disk_SaveLastDiskImage( iDrive );
	
	return Error;
}

//===========================================================================

BOOL DiskIsSpinning(void)
{
	return floppymotoron;
}

//===========================================================================

void DiskNotifyInvalidImage(const int iDrive, LPCTSTR pszImageFilename, const ImageError_e Error)
{
	TCHAR szBuffer[MAX_PATH+128];

	switch (Error)
	{
	case eIMAGE_ERROR_UNABLE_TO_OPEN:
	case eIMAGE_ERROR_UNABLE_TO_OPEN_GZ:
	case eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP:
		wsprintf(
			szBuffer,
			TEXT("Unable to open the file %s."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_BAD_SIZE:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("disk image is an unsupported size."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_BAD_FILE:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("OS can't access it."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("disk image format is not recognized."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED_HDV:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the file %s\n")
			TEXT("because this UniDisk 3.5/Apple IIGS/hard-disk image is not supported.\n")
			TEXT("Try inserting as a hard-disk image instead."),
			pszImageFilename);
		break;

	case eIMAGE_ERROR_UNSUPPORTED_MULTI_ZIP:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("first file (%s) in this multi-zip archive is not recognized.\n")
			TEXT("Try unzipping and using the disk images directly.\n"),
			pszImageFilename,
			g_aFloppyDisk[iDrive].strFilenameInZip.c_str());
		break;

	case eIMAGE_ERROR_GZ:
	case eIMAGE_ERROR_ZIP:
		wsprintf(
			szBuffer,
			TEXT("Unable to use the compressed file %s\nbecause the ")
			TEXT("compressed disk image is corrupt/unsupported."),
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
		Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];
		if (pFloppy->bWriteProtected)
			return true;
	}
	return false;
}


//===========================================================================

void DiskSetProtect(const int iDrive, const bool bWriteProtect)
{
	if (IsDriveValid( iDrive ))
	{
		Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];
		pFloppy->bWriteProtected = bWriteProtect;
	}
}


//===========================================================================

bool Disk_ImageIsWriteProtected(const int iDrive)
{
	if (!IsDriveValid(iDrive))
		return true;

	Disk_t *pFloppy = &g_aFloppyDisk[iDrive];
	return ImageIsWriteProtected(pFloppy->imagehandle);
}

//===========================================================================

bool Disk_IsDriveEmpty(const int iDrive)
{
	if (!IsDriveValid(iDrive))
		return true;

	Disk_t *pFloppy = &g_aFloppyDisk[iDrive];
	return pFloppy->imagehandle == NULL;
}

//===========================================================================

static void __stdcall DiskReadWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	/* floppyloadmode = 0; */
	Disk_t * fptr = &g_aFloppyDisk[currdrive];

	diskaccessed = 1;

	if (!fptr->trackimagedata && fptr->imagehandle)
		ReadTrack(currdrive);

	if (!fptr->trackimagedata)
	{
		floppylatch = 0xFF;
		return;
	}

	if (!floppywritemode)
	{
		floppylatch = *(fptr->trackimage + fptr->byte);
#if LOG_DISK_NIBBLES
		LOG_DISK("read %4X = %2X\r\n", fptr->byte, floppylatch);
#endif
	}
	else if ((floppylatch & 0x80) && !fptr->bWriteProtected) // && floppywritemode
	{
		*(fptr->trackimage + fptr->byte) = floppylatch;
		fptr->trackimagedirty = 1;
	}

	if (++fptr->byte >= fptr->nibbles)
		fptr->byte = 0;

	// Feature Request #201 Show track status
	// https://github.com/AppleWin/AppleWin/issues/201
	// NB. Prevent flooding of forcing UI to redraw!!!
	if( ((fptr->byte) & 0xFF) == 0 )
		FrameDrawDiskStatus( (HDC)0 ); 
}

//===========================================================================

void DiskReset(void)
{
	floppymotoron = 0;
	phases = 0;
}

//===========================================================================

void DiskSelectImage(const int iDrive, LPSTR pszFilename)
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

	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			_tcscat(filename,TEXT(".dsk"));

		ImageError_e Error = DiskInsert(iDrive, filename, ofn.Flags & OFN_READONLY, IMAGE_CREATE);
		if (Error == eIMAGE_ERROR_NONE)
		{
			DiskSetDiskPathFilename(iDrive, filename); 
			filename[ofn.nFileOffset] = 0;
			if (_tcsicmp(directory, filename))
				RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, filename);
		}
		else
		{
			DiskNotifyInvalidImage(iDrive, filename, Error);
		}
	}
}

//===========================================================================

void DiskSelect(const int iDrive)
{
	DiskSelectImage(iDrive, TEXT(""));
}

//===========================================================================

static void __stdcall DiskLoadWriteProtect(WORD, WORD, BYTE write, BYTE value, ULONG) {
	/* floppyloadmode = 1; */
	if (!write)
	{
		if (floppymotoron && !floppywritemode)
		{
			// phase 1 on also forces write protect in the Disk II drive (UTA2E page 9-7) but we don't implement that
			if (g_aFloppyDisk[currdrive].bWriteProtected)
				floppylatch |= 0x80;
			else
				floppylatch &= 0x7F;
		}
	}
}

//===========================================================================

static void __stdcall DiskSetReadMode(WORD, WORD, BYTE, BYTE, ULONG)
{
	floppywritemode = 0;
}

//===========================================================================

static void __stdcall DiskSetWriteMode(WORD, WORD, BYTE, BYTE, ULONG uExecutedCycles)
{
	floppywritemode = 1;
	BOOL modechange = !g_aFloppyDisk[currdrive].writelight;
	g_aFloppyDisk[currdrive].writelight = 20000;
	if (modechange)
	{
		//FrameRefreshStatus(DRAW_LEDS);
		FrameDrawDiskLEDS( (HDC)0 );
	}
}

//===========================================================================

void DiskUpdatePosition(DWORD cycles)
{
	int loop = NUM_DRIVES;
	while (loop--)
	{
		Disk_t * fptr = &g_aFloppyDisk[loop];

		if (fptr->spinning && !floppymotoron) {
			if (!(fptr->spinning -= MIN(fptr->spinning, (cycles >> 6))))
			{
				// FrameRefreshStatus(DRAW_LEDS);
				FrameDrawDiskLEDS( (HDC)0 );
				FrameDrawDiskStatus( (HDC)0 );
			}
		}

		if (floppywritemode && (currdrive == loop) && fptr->spinning)
		{
			fptr->writelight = 20000;
		}
		else if (fptr->writelight)
		{
			if (!(fptr->writelight -= MIN(fptr->writelight, (cycles >> 6))))
			{
				//FrameRefreshStatus(DRAW_LEDS);
				FrameDrawDiskLEDS( (HDC)0 );
				FrameDrawDiskStatus( (HDC)0 );
			}
		}

		if ((!enhancedisk) && (!diskaccessed) && fptr->spinning)
		{
			fptr->byte += (cycles >> 5);
			if (fptr->byte >= fptr->nibbles)
				fptr->byte -= fptr->nibbles;
		}
	}

	diskaccessed = 0;
}

//===========================================================================

bool DiskDriveSwap(void)
{
	// Refuse to swap if either Disk][ is active
	if(g_aFloppyDisk[0].spinning || g_aFloppyDisk[1].spinning)
		return false;

	// Swap disks between drives
	// . NB. We swap trackimage ptrs (so don't need to swap the buffers' data)
	// . TODO: Consider array of Pointers: Disk_t* g_aDrive[]
	std::swap(g_aFloppyDisk[0], g_aFloppyDisk[1]);

	Disk_SaveLastDiskImage(DRIVE_1);
	Disk_SaveLastDiskImage(DRIVE_2);

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES, false );

	return true;
}

//===========================================================================

static BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

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
}

//===========================================================================

static BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xF)
	{
	case 0x0:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x1:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x2:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x3:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x4:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x5:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x6:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x7:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x8:	DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x9:	DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xA:	DiskEnable(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xB:	DiskEnable(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xC:	DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xD:	DiskLoadWriteProtect(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xE:	DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xF:	DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft); break;
	}

	// only even addresses return the latch (UTA2E Table 9.1)
	if (!(addr & 1))
		return floppylatch;
	else
		return MemReadFloatingBus(nCyclesLeft);
}

static BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xF)
	{
	case 0x0:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x1:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x2:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x3:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x4:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x5:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x6:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x7:	DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x8:	DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0x9:	DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xA:	DiskEnable(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xB:	DiskEnable(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xC:	DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xD:	DiskLoadWriteProtect(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xE:	DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft); break;
	case 0xF:	DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft); break;
	}

	// any address writes the latch via sequencer LD command (74LS323 datasheet)
	if (floppywritemode /* && floppyloadmode */)
	{
		floppylatch = d;
	}
	return 0;
}

//===========================================================================

DWORD DiskGetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot)
{
	pSS->Hdr.UnitHdr.dwLength = sizeof(SS_CARD_DISK2);
	pSS->Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,2);

	pSS->Hdr.dwSlot = dwSlot;
	pSS->Hdr.dwType = CT_Disk2;

	pSS->phases			    = phases; // new in 1.0.0.2 disk snapshots
	pSS->currdrive			= currdrive; // this was an int in 1.0.0.1 disk snapshots
	pSS->diskaccessed		= diskaccessed;
	pSS->enhancedisk		= enhancedisk;
	pSS->floppylatch		= floppylatch;
	pSS->floppymotoron		= floppymotoron;
	pSS->floppywritemode	= floppywritemode;

	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		strcpy(pSS->Unit[i].szFileName, g_aFloppyDisk[i].fullname);
		pSS->Unit[i].track				= g_aFloppyDisk[i].track;
		pSS->Unit[i].phase				= g_aFloppyDisk[i].phase;
		pSS->Unit[i].byte				= g_aFloppyDisk[i].byte;
		pSS->Unit[i].writeprotected		= g_aFloppyDisk[i].bWriteProtected ? TRUE : FALSE;
		pSS->Unit[i].trackimagedata		= g_aFloppyDisk[i].trackimagedata;
		pSS->Unit[i].trackimagedirty	= g_aFloppyDisk[i].trackimagedirty;
		pSS->Unit[i].spinning			= g_aFloppyDisk[i].spinning;
		pSS->Unit[i].writelight			= g_aFloppyDisk[i].writelight;
		pSS->Unit[i].nibbles			= g_aFloppyDisk[i].nibbles;

		if(g_aFloppyDisk[i].trackimage)
			memcpy(pSS->Unit[i].nTrack, g_aFloppyDisk[i].trackimage, NIBBLES_PER_TRACK);
		else
			memset(pSS->Unit[i].nTrack, 0, NIBBLES_PER_TRACK);
	}

	return 0;
}

DWORD DiskSetSnapshot(SS_CARD_DISK2* pSS, DWORD /*dwSlot*/)
{
	if(pSS->Hdr.UnitHdr.dwVersion > MAKE_VERSION(1,0,0,2))
    {
        return -1;
    }

	phases  		= pSS->phases; // new in 1.0.0.2 disk snapshots
	currdrive		= pSS->currdrive; // this was an int in 1.0.0.1 disk snapshots
	diskaccessed	= pSS->diskaccessed;
	enhancedisk		= pSS->enhancedisk;
	floppylatch		= pSS->floppylatch;
	floppymotoron	= pSS->floppymotoron;
	floppywritemode	= pSS->floppywritemode;

	// Eject all disks first in case Drive-2 contains disk to be inserted into Drive-1
	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		DiskEject(i);	// Remove any disk & update Registry to reflect empty drive
		ZeroMemory(&g_aFloppyDisk[i], sizeof(Disk_t ));
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
			// . strDiskPathFilename
			// . writeprotected
		}

		//

//		strcpy(g_aFloppyDisk[i].fullname, pSS->Unit[i].szFileName);
		g_aFloppyDisk[i].track				= pSS->Unit[i].track;
		g_aFloppyDisk[i].phase				= pSS->Unit[i].phase;
		g_aFloppyDisk[i].byte				= pSS->Unit[i].byte;
//		g_aFloppyDisk[i].writeprotected		= pSS->Unit[i].writeprotected;
		g_aFloppyDisk[i].trackimagedata		= pSS->Unit[i].trackimagedata;
		g_aFloppyDisk[i].trackimagedirty	= pSS->Unit[i].trackimagedirty;
		g_aFloppyDisk[i].spinning			= pSS->Unit[i].spinning;
		g_aFloppyDisk[i].writelight			= pSS->Unit[i].writelight;
		g_aFloppyDisk[i].nibbles			= pSS->Unit[i].nibbles;

		//

		if(!bImageError)
		{
			if((g_aFloppyDisk[i].trackimage == NULL) && g_aFloppyDisk[i].nibbles)
				AllocTrack(i);

			if(g_aFloppyDisk[i].trackimage == NULL)
				bImageError = true;
			else
				memcpy(g_aFloppyDisk[i].trackimage, pSS->Unit[i].nTrack, NIBBLES_PER_TRACK);
		}

		if(bImageError)
		{
			g_aFloppyDisk[i].trackimagedata		= 0;
			g_aFloppyDisk[i].trackimagedirty	= 0;
			g_aFloppyDisk[i].nibbles			= 0;
		}
	}

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	return 0;
}
