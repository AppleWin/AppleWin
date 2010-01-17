/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
 */

#include "StdAfx.h"
#include "DiskImage.h"
#pragma  hdrstop
#include "..\resource\resource.h"

#define LOG_DISK_ENABLED 0

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
	string DiskPathFilename[NUM_DRIVES];	// TODO: Move this into Disk_t & add accessor funcs

// Private ________________________________________________________________________________________

	const int MAX_DISK_IMAGE_NAME = 15;
	const int MAX_DISK_FULL_NAME  = 127;

	struct Disk_t
	{
		TCHAR  imagename[ MAX_DISK_IMAGE_NAME + 1 ];	// <FILENAME> (ie. no extension)
		TCHAR  fullname [ MAX_DISK_FULL_NAME  + 1 ];	// <FILENAME.EXT> or <FILENAME.zip>  : This is persisted to the snapshot file
		string strFilenameInZip;						// 0x00           or <FILENAME.EXT>
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
	};

static WORD		currdrive       = 0;
static BOOL		diskaccessed    = 0;
static Disk_t	g_aFloppyDisk[NUM_DRIVES];
static BYTE		floppylatch     = 0;
static BOOL		floppymotoron   = 0;
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

void Disk_LoadLastDiskImage(const int iDrive)
{
	char sFilePath[ MAX_PATH + 1];
	sFilePath[0] = 0;

	char *pRegKey = (iDrive == DRIVE_1)
		? REGVALUE_PREF_LAST_DISK_1
		: REGVALUE_PREF_LAST_DISK_2;

	if (RegLoadString(TEXT(REG_PREFS),pRegKey,1,sFilePath,MAX_PATH))
	{
		sFilePath[ MAX_PATH ] = 0;
		DiskPathFilename[ iDrive ] = sFilePath;

#if _DEBUG
//		MessageBox(NULL,pFileName,pRegKey,MB_OK);
#endif

		//	_tcscat(imagefilename,TEXT("MASTER.DSK")); // TODO: Should remember last disk by user
		g_bSaveDiskImage = false;
		// Pass in ptr to local copy of filepath, since RemoveDisk() sets DiskPathFilename = ""
		DiskInsert(iDrive, sFilePath, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
		g_bSaveDiskImage = true;
	}
	//else MessageBox(NULL,"Reg Key/Value not found",pRegKey,MB_OK);
}

//===========================================================================

void Disk_SaveLastDiskImage(const int iDrive)
{
	const char *pFileName = DiskPathFilename[iDrive].c_str();

	if (g_bSaveDiskImage)
	{
		if (iDrive == DRIVE_1)
			RegSaveString(TEXT(REG_PREFS),REGVALUE_PREF_LAST_DISK_1,1,pFileName);
		else
			RegSaveString(TEXT(REG_PREFS),REGVALUE_PREF_LAST_DISK_2,1,pFileName);
	}
}

//===========================================================================

static void CheckSpinning(void)
{
	DWORD modechange = (floppymotoron && !g_aFloppyDisk[currdrive].spinning);
	if (floppymotoron)
		g_aFloppyDisk[currdrive].spinning = 20000;
	if (modechange)
		FrameRefreshStatus(DRAW_LEDS);
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
		LOG_DISK("read track %2X%s\r", pFloppy->track, (pFloppy->phase & 1) ? ".5" : "");

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
	DiskPathFilename[iDrive] = "";

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
		ImageWriteTrack(
			pFloppy->imagehandle,
			pFloppy->track,
			pFloppy->phase,
			pFloppy->trackimage,
			pFloppy->nibbles );

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

static BYTE __stdcall DiskControlMotor(WORD, WORD address, BYTE, BYTE, ULONG)
{
	floppymotoron = address & 1;
	CheckSpinning();
	return MemReturnRandomData(1);
}

//===========================================================================

static BYTE __stdcall DiskControlStepper(WORD, WORD address, BYTE, BYTE, ULONG)
{
	Disk_t * fptr = &g_aFloppyDisk[currdrive];
	int phase     = (address >> 1) & 3;
	int phase_bit = (1 << phase);

	// update the magnet states
	if (address & 1)
	{
		// phase on
		phases |= phase_bit;
		LOG_DISK("track %02X phases %X phase %d on  address $C0E%X\r", fptr->phase, phases, phase, address & 0xF);
	}
	else
	{
		// phase off
		phases &= ~phase_bit;
		LOG_DISK("track %02X phases %X phase %d off address $C0E%X\r", fptr->phase, phases, phase, address & 0xF);
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
		LOG_DISK("newtrack %2X%s\r", newtrack, (fptr->phase & 1) ? ".5" : "");
		if (newtrack != fptr->track)
		{
			if (fptr->trackimage && fptr->trackimagedirty)
			{
				WriteTrack(currdrive);
			}
			fptr->track          = newtrack;
			fptr->trackimagedata = 0;
		}
	}

	return (address == 0xE0) ? 0xFF : MemReturnRandomData(1);
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

static BYTE __stdcall DiskEnable(WORD, WORD address, BYTE, BYTE, ULONG)
{
	currdrive = address & 1;
	g_aFloppyDisk[!currdrive].spinning   = 0;
	g_aFloppyDisk[!currdrive].writelight = 0;
	CheckSpinning();
	return 0;
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

void DiskGetLightStatus(int *pDisk1Status_, int *pDisk2Status_)
{
//	*drive1 = g_aFloppyDisk[0].spinning ? g_aFloppyDisk[0].writelight ? 2 : 1 : 0;
//	*drive2 = g_aFloppyDisk[1].spinning ? g_aFloppyDisk[1].writelight ? 2 : 1 : 0;

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

	ZeroMemory(fptr,sizeof(Disk_t ));

	const DWORD dwAttributes = GetFileAttributes(pszImageFilename);
	if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		fptr->bWriteProtected = false;	// Assume this is a new file to create
	else
		fptr->bWriteProtected = bForceWriteProtected ? true : (dwAttributes & FILE_ATTRIBUTE_READONLY);

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

		DiskPathFilename[iDrive] = pszImageFilename;

		//MessageBox( NULL, imagefilename, fptr->imagename, MB_OK );
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

static BYTE __stdcall DiskReadWrite (WORD programcounter, WORD, BYTE, BYTE, ULONG)
{
	Disk_t * fptr = &g_aFloppyDisk[currdrive];

	diskaccessed = 1;

	if (!fptr->trackimagedata && fptr->imagehandle)
		ReadTrack(currdrive);

	if (!fptr->trackimagedata)
		return 0xFF;

	BYTE result = 0;

	if (!floppywritemode || !fptr->bWriteProtected)
	{
		if (floppywritemode)
		{
			if (floppylatch & 0x80)
			{
				*(fptr->trackimage+fptr->byte) = floppylatch;
				fptr->trackimagedirty = 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			result = *(fptr->trackimage+fptr->byte);
		}
	}

	if (0)
		{ LOG_DISK("nib %4X = %2X\r", fptr->byte, result); }

	if (++fptr->byte >= fptr->nibbles)
		fptr->byte = 0;

	return result;
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
			DiskPathFilename[iDrive] = filename; 
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

static BYTE __stdcall DiskSetLatchValue(WORD, WORD, BYTE write, BYTE value, ULONG) {
	if (write)
		floppylatch = value;
	return floppylatch;
}

//===========================================================================

static BYTE __stdcall DiskSetReadMode(WORD, WORD, BYTE, BYTE, ULONG)
{
	floppywritemode = 0;
	return MemReturnRandomData(g_aFloppyDisk[currdrive].bWriteProtected);
}

//===========================================================================

static BYTE __stdcall DiskSetWriteMode(WORD, WORD, BYTE, BYTE, ULONG)
{
	floppywritemode = 1;
	BOOL modechange = !g_aFloppyDisk[currdrive].writelight;
	g_aFloppyDisk[currdrive].writelight = 20000;
	if (modechange)
		FrameRefreshStatus(DRAW_LEDS);
	return MemReturnRandomData(1);
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
				FrameRefreshStatus(DRAW_LEDS);
		}

		if (floppywritemode && (currdrive == loop) && fptr->spinning)
		{
			fptr->writelight = 20000;
		}
		else if (fptr->writelight)
		{
			if (!(fptr->writelight -= MIN(fptr->writelight, (cycles >> 6))))
				FrameRefreshStatus(DRAW_LEDS);
		}

		if ((!enhancedisk) && (!diskaccessed) && fptr->spinning)
		{
			needsprecision = cumulativecycles;
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
	Disk_t  temp;

	// Swap trackimage ptrs (so don't need to swap the buffers' data)
	// TODO: Array of Pointers: Disk_t* g_aDrive[]
	memcpy(&temp            , &g_aFloppyDisk[0], sizeof(Disk_t ));
	memcpy(&g_aFloppyDisk[0], &g_aFloppyDisk[1], sizeof(Disk_t ));
	memcpy(&g_aFloppyDisk[1], &temp            , sizeof(Disk_t ));

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

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

	// NB. We used to disable the track stepping delay in the Disk II controller firmware by
	// patching $C64C with $A9,$00,$EA. Now not doing this since:
	// . Authentic Speed should be authentic
	// . Enhanced Speed runs emulation unthrottled, so removing the delay has negligible effect
	// . Patching the firmware breaks the ADC checksum used by "The CIA Files" (Tricky Dick)
	// . In this case we can patch to compensate for an ADC or EOR checksum but not both

	RegisterIoHandler(uSlot, Disk_IORead, Disk_IOWrite, NULL, NULL, NULL, NULL);
}

//===========================================================================

static BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	addr &= 0xFF;

	switch (addr & 0xf)
	{
	case 0x0:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return DiskSetLatchValue(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

static BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	addr &= 0xFF;

	switch (addr & 0xf)
	{
	case 0x0:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return DiskControlStepper(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return DiskControlMotor(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return DiskEnable(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return DiskReadWrite(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return DiskSetLatchValue(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return DiskSetReadMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return DiskSetWriteMode(pc, addr, bWrite, d, nCyclesLeft);
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

	for(UINT i=0; i<NUM_DRIVES; i++)
	{
		bool bImageError = false;

		ZeroMemory(&g_aFloppyDisk[i], sizeof(Disk_t ));
		if(pSS->Unit[i].szFileName[0] == 0x00)
			continue;

		//

		DWORD dwAttributes = GetFileAttributes(pSS->Unit[i].szFileName);
		if(dwAttributes == INVALID_FILE_ATTRIBUTES)
		{
			// Get user to browse for file
			DiskSelectImage(i, pSS->Unit[i].szFileName);

			dwAttributes = GetFileAttributes(pSS->Unit[i].szFileName);
		}

		if(dwAttributes != INVALID_FILE_ATTRIBUTES)
		{
			if(DiskInsert(i, pSS->Unit[i].szFileName, dwAttributes & FILE_ATTRIBUTE_READONLY, IMAGE_DONT_CREATE) != eIMAGE_ERROR_NONE)
				bImageError = true;

			// DiskInsert() sets up:
			// . fullname
			// . imagename
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
