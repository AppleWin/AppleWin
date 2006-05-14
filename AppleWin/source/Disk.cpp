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

/* Description: Disk
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma  hdrstop

// Public _________________________________________________________________________________________

	BOOL      enhancedisk     = 1;

// Private ________________________________________________________________________________________

	const int MAX_DISK_IMAGE_NAME = 15;
	const int MAX_DISK_FULL_NAME  = 127;

	struct Disk_t
	{
		TCHAR  imagename[ MAX_DISK_IMAGE_NAME + 1 ];
		TCHAR  fullname [ MAX_DISK_FULL_NAME  + 1 ];
		HIMAGE imagehandle;
		int    track;
		LPBYTE trackimage;
		int    phase;
		int    byte;
		BOOL   writeprotected;
		BOOL   trackimagedata;
		BOOL   trackimagedirty;
		DWORD  spinning;
		DWORD  writelight;
		int    nibbles;
	};

static int       currdrive       = 0;
static BOOL      diskaccessed    = 0;
static Disk_t    g_aFloppyDisk[DRIVES];
static BYTE      floppylatch     = 0;
static BOOL      floppymotoron   = 0;
static BOOL      floppywritemode = 0;

static void ChecSpinning();
static Disk_Status_e GetDriveLightStatus( const int iDrive );
static bool IsDriveValid( const int iDrive );
static void ReadTrack (int drive);
static void RemoveDisk (int drive);
static void WriteTrack (int drive);

//===========================================================================
void CheckSpinning () {
  DWORD modechange = (floppymotoron && !g_aFloppyDisk[currdrive].spinning);
  if (floppymotoron)
    g_aFloppyDisk[currdrive].spinning = 20000;
  if (modechange)
    FrameRefreshStatus(DRAW_LEDS);
}

//===========================================================================
Disk_Status_e GetDriveLightStatus( const int iDrive )
{
	if (IsDriveValid( iDrive ))
	{
		Disk_t *pFloppy = & g_aFloppyDisk[ iDrive ];

		if (pFloppy->spinning)
		{
			if (pFloppy->writeprotected)
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
void GetImageTitle (LPCTSTR imagefilename, Disk_t * fptr)
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
bool IsDriveValid( const int iDrive )
{
	if (iDrive < 0)
		return false;

	if (iDrive >= DRIVES)
		return false;

	return true;
}


//===========================================================================

static void AllocTrack(int drive)
{
  Disk_t * fptr = &g_aFloppyDisk[drive];
  fptr->trackimage = (LPBYTE)VirtualAlloc(NULL,NIBBLES_PER_TRACK,MEM_COMMIT,PAGE_READWRITE);
}

//===========================================================================
static void ReadTrack (int iDrive)
{
	if (! IsDriveValid( iDrive ))
		return;

	Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];

	if (pFloppy->track >= TRACKS)
	{
		pFloppy->trackimagedata = 0;
		return;
	}

	if (! pFloppy->trackimage)
		AllocTrack( iDrive );

	if (pFloppy->trackimage && pFloppy->imagehandle)
	{
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
static void RemoveDisk (int iDrive)
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
}

//===========================================================================
static void WriteTrack (int iDrive)
{
	Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];

	if (pFloppy->track >= TRACKS)
		return;

	if (pFloppy->writeprotected)
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
void DiskBoot () {

  // THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
  // IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
  if (g_aFloppyDisk[0].imagehandle && ImageBoot(g_aFloppyDisk[0].imagehandle))
    floppymotoron = 0;

}

//===========================================================================
BYTE __stdcall DiskControlMotor (WORD, BYTE address, BYTE, BYTE, ULONG) {
  floppymotoron = address & 1;
  CheckSpinning();
  return MemReturnRandomData(1);
}

//===========================================================================
BYTE __stdcall DiskControlStepper (WORD, BYTE address, BYTE, BYTE, ULONG) {
  Disk_t * fptr = &g_aFloppyDisk[currdrive];
  if (address & 1) {
    int phase     = (address >> 1) & 3;
    int direction = 0;
    if (phase == ((fptr->phase+1) & 3))
      direction = 1;
    if (phase == ((fptr->phase+3) & 3))
      direction = -1;
    if (direction) {
      fptr->phase = MAX(0,MIN(79,fptr->phase+direction));
      if (!(fptr->phase & 1)) {
        int newtrack = MIN(TRACKS-1,fptr->phase >> 1);
        if (newtrack != fptr->track) {
          if (fptr->trackimage && fptr->trackimagedirty)
            WriteTrack(currdrive);
          fptr->track          = newtrack;
          fptr->trackimagedata = 0;
        }
      }
    }
  }
  return (address == 0xE0) ? 0xFF : MemReturnRandomData(1);
}

//===========================================================================
void DiskDestroy ()
{
	RemoveDisk(0);
	RemoveDisk(1);
}

//===========================================================================
BYTE __stdcall DiskEnable (WORD, BYTE address, BYTE, BYTE, ULONG) {
  currdrive = address & 1;
  g_aFloppyDisk[!currdrive].spinning   = 0;
  g_aFloppyDisk[!currdrive].writelight = 0;
  CheckSpinning();
  return 0;
}


//===========================================================================
void DiskEject( const int iDrive )
{
	if (IsDriveValid( iDrive ))
	{
		RemoveDisk( iDrive );
	}
}


//===========================================================================
LPCTSTR DiskGetFullName (int drive) {
  return g_aFloppyDisk[drive].fullname;
}


//===========================================================================
void DiskGetLightStatus (int *pDisk1Status_, int *pDisk2Status_)
{
//	*drive1 = g_aFloppyDisk[0].spinning ? g_aFloppyDisk[0].writelight ? 2 : 1 : 0;
//	*drive2 = g_aFloppyDisk[1].spinning ? g_aFloppyDisk[1].writelight ? 2 : 1 : 0;

	if (pDisk1Status_)
		*pDisk1Status_ = GetDriveLightStatus( 0 );
	if (pDisk2Status_)
		*pDisk2Status_ = GetDriveLightStatus( 1 );
}

//===========================================================================
LPCTSTR DiskGetName (int drive) {
  return g_aFloppyDisk[drive].imagename;
}

//===========================================================================
void DiskInitialize () {
  int loop = DRIVES;
  while (loop--)
    ZeroMemory(&g_aFloppyDisk[loop],sizeof(Disk_t ));
  TCHAR imagefilename[MAX_PATH];
  _tcscpy(imagefilename,progdir);
  _tcscat(imagefilename,TEXT("MASTER.DSK")); // TODO: Should remember last disk by user
  DiskInsert(0,imagefilename,0,0);
}

//===========================================================================
int DiskInsert (int drive, LPCTSTR imagefilename, BOOL writeprotected, BOOL createifnecessary) {
  Disk_t * fptr = &g_aFloppyDisk[drive];
  if (fptr->imagehandle)
    RemoveDisk(drive);
  ZeroMemory(fptr,sizeof(Disk_t ));
  fptr->writeprotected = writeprotected;
  int error = ImageOpen(imagefilename,
                        &fptr->imagehandle,
                        &fptr->writeprotected,
                        createifnecessary);
  if (error == IMAGE_ERROR_NONE)
    GetImageTitle(imagefilename,fptr);
  return error;
}

//===========================================================================
BOOL DiskIsSpinning () {
  return floppymotoron;
}

//===========================================================================
void DiskNotifyInvalidImage (LPCTSTR imagefilename,int error)
{
	TCHAR buffer[MAX_PATH+128];

  switch (error) {

    case 1:
      wsprintf(buffer,
               TEXT("Unable to open the file %s."),
               (LPCTSTR)imagefilename);
      break;

    case 2:
      wsprintf(buffer,
               TEXT("Unable to use the file %s\nbecause the ")
               TEXT("disk image format is not recognized."),
               (LPCTSTR)imagefilename);
      break;

    default:

      // IGNORE OTHER ERRORS SILENTLY
      return;
  }
  MessageBox(g_hFrameWindow,
             buffer,
             TITLE,
             MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}


//===========================================================================
bool DiskGetProtect( const int iDrive )
{
	if (IsDriveValid( iDrive ))
	{
		Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];
		if (pFloppy->writeprotected)
			return true;
	}
	return false;
}


//===========================================================================
void DiskSetProtect( const int iDrive, const bool bWriteProtect )
{
	if (IsDriveValid( iDrive ))
	{
		Disk_t *pFloppy = &g_aFloppyDisk[ iDrive ];
		pFloppy->writeprotected = bWriteProtect;
	}
}


//===========================================================================
BYTE __stdcall DiskReadWrite (WORD programcounter, BYTE, BYTE, BYTE, ULONG) {
  Disk_t * fptr = &g_aFloppyDisk[currdrive];
  diskaccessed = 1;
  if ((!fptr->trackimagedata) && fptr->imagehandle)
    ReadTrack(currdrive);
  if (!fptr->trackimagedata)
    return 0xFF;
  BYTE result = 0;
  if ((!floppywritemode) || (!fptr->writeprotected))
    if (floppywritemode)
      if (floppylatch & 0x80) {
        *(fptr->trackimage+fptr->byte) = floppylatch;
        fptr->trackimagedirty = 1;
      }
      else
        return 0;
    else
      result = *(fptr->trackimage+fptr->byte);
  if (++fptr->byte >= fptr->nibbles)
    fptr->byte = 0;
  return result;
}

//===========================================================================
void DiskReset () {
  floppymotoron = 0;
}

//===========================================================================
void DiskSelectImage (int drive, LPSTR pszFilename)
{
  TCHAR directory[MAX_PATH] = TEXT("");
  TCHAR filename[MAX_PATH];
  TCHAR title[40];

  strcpy(filename, pszFilename);

  RegLoadString(TEXT("Preferences"),TEXT("Starting Directory"),1,directory,MAX_PATH);
  _tcscpy(title,TEXT("Select Disk Image For Drive "));
  _tcscat(title,drive ? TEXT("2") : TEXT("1"));

  OPENFILENAME ofn;
  ZeroMemory(&ofn,sizeof(OPENFILENAME));
  ofn.lStructSize     = sizeof(OPENFILENAME);
  ofn.hwndOwner       = g_hFrameWindow;
  ofn.hInstance       = instance;
  ofn.lpstrFilter     = TEXT("All Images\0*.apl;*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0")
                        TEXT("Disk Images (*.bin,*.do,*.dsk,*.iie,*.nib,*.po)\0*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0")
                        TEXT("All Files\0*.*\0");
  ofn.lpstrFile       = filename;
  ofn.nMaxFile        = MAX_PATH;
  ofn.lpstrInitialDir = directory;
  ofn.Flags           = OFN_PATHMUSTEXIST;
  ofn.lpstrTitle      = title;

  if (GetOpenFileName(&ofn))
  {
    if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
      _tcscat(filename,TEXT(".DSK"));

    int error = DiskInsert(drive,filename,ofn.Flags & OFN_READONLY,1);
    if (!error)
	{
      filename[ofn.nFileOffset] = 0;
      if (_tcsicmp(directory,filename))
        RegSaveString(TEXT("Preferences"),TEXT("Starting Directory"),1,filename);
    }
    else
	{
      DiskNotifyInvalidImage(filename,error);
	}
  }
}

//===========================================================================
void DiskSelect (int drive)
{
	DiskSelectImage(drive, TEXT(""));
}

//===========================================================================
BYTE __stdcall DiskSetLatchValue (WORD, BYTE, BYTE write, BYTE value, ULONG) {
  if (write)
    floppylatch = value;
  return floppylatch;
}

//===========================================================================
BYTE __stdcall DiskSetReadMode (WORD, BYTE, BYTE, BYTE, ULONG) {
  floppywritemode = 0;
  return MemReturnRandomData(g_aFloppyDisk[currdrive].writeprotected);
}

//===========================================================================
BYTE __stdcall DiskSetWriteMode (WORD, BYTE, BYTE, BYTE, ULONG) {
  floppywritemode = 1;
  BOOL modechange = !g_aFloppyDisk[currdrive].writelight;
  g_aFloppyDisk[currdrive].writelight = 20000;
  if (modechange)
    FrameRefreshStatus(DRAW_LEDS);
  return MemReturnRandomData(1);
}

//===========================================================================
void DiskUpdatePosition (DWORD cycles) {
  int loop = 2;
  while (loop--) {
    Disk_t * fptr = &g_aFloppyDisk[loop];
    if (fptr->spinning && !floppymotoron) {
      if (!(fptr->spinning -= MIN(fptr->spinning,(cycles >> 6))))
        FrameRefreshStatus(DRAW_LEDS);
    }
    if (floppywritemode && (currdrive == loop) && fptr->spinning)
      fptr->writelight = 20000;
    else if (fptr->writelight) {
      if (!(fptr->writelight -= MIN(fptr->writelight,(cycles >> 6))))
        FrameRefreshStatus(DRAW_LEDS);
    }
    if ((!enhancedisk) && (!diskaccessed) && fptr->spinning) {
      needsprecision = cumulativecycles;
      fptr->byte += (cycles >> 5);
      if (fptr->byte >= fptr->nibbles)
        fptr->byte -= fptr->nibbles;
    }
  }
  diskaccessed = 0;
}

//===========================================================================

bool DiskDriveSwap()
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

DWORD DiskGetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot)
{
	pSS->Hdr.UnitHdr.dwLength = sizeof(SS_CARD_DISK2);
	pSS->Hdr.UnitHdr.dwVersion = MAKE_VERSION(1,0,0,1);

	pSS->Hdr.dwSlot = dwSlot;
	pSS->Hdr.dwType = CT_Disk2;

	pSS->currdrive			= currdrive;
	pSS->diskaccessed		= diskaccessed;
	pSS->enhancedisk		= enhancedisk;
	pSS->floppylatch		= floppylatch;
	pSS->floppymotoron		= floppymotoron;
	pSS->floppywritemode	= floppywritemode;

	for(UINT i=0; i<2; i++)
	{
		strcpy(pSS->Unit[i].szFileName, g_aFloppyDisk[i].fullname);
		pSS->Unit[i].track				= g_aFloppyDisk[i].track;
		pSS->Unit[i].phase				= g_aFloppyDisk[i].phase;
		pSS->Unit[i].byte				= g_aFloppyDisk[i].byte;
		pSS->Unit[i].writeprotected		= g_aFloppyDisk[i].writeprotected;
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
	if(pSS->Hdr.UnitHdr.dwVersion != MAKE_VERSION(1,0,0,1))
		return -1;

	currdrive		= pSS->currdrive;
	diskaccessed	= pSS->diskaccessed;
	enhancedisk		= pSS->enhancedisk;
	floppylatch		= pSS->floppylatch;
	floppymotoron	= pSS->floppymotoron;
	floppywritemode	= pSS->floppywritemode;

	for(UINT i=0; i<2; i++)
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
			BOOL bWriteProtected = (dwAttributes & FILE_ATTRIBUTE_READONLY) ? TRUE : FALSE;

			if(DiskInsert(i, pSS->Unit[i].szFileName, bWriteProtected, 0))
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
//		g_aFloppyDisk[i].writeprotected	= pSS->Unit[i].writeprotected;
		g_aFloppyDisk[i].trackimagedata	= pSS->Unit[i].trackimagedata;
		g_aFloppyDisk[i].trackimagedirty	= pSS->Unit[i].trackimagedirty;
		g_aFloppyDisk[i].spinning			= pSS->Unit[i].spinning;
		g_aFloppyDisk[i].writelight		= pSS->Unit[i].writelight;
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
			g_aFloppyDisk[i].trackimagedata	= 0;
			g_aFloppyDisk[i].trackimagedirty	= 0;
			g_aFloppyDisk[i].nibbles			= 0;
		}
	}

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	return 0;
}
