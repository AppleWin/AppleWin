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

typedef struct _floppyrec {
    TCHAR  imagename[16];
    TCHAR  fullname[128];
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
} floppyrec, *floppyptr;

static int       currdrive       = 0;
static BOOL      diskaccessed    = 0;
BOOL      enhancedisk     = 1;
static floppyrec floppy[DRIVES];
static BYTE      floppylatch     = 0;
static BOOL      floppymotoron   = 0;
static BOOL      floppywritemode = 0;

static void ReadTrack (int drive);
static void RemoveDisk (int drive);
static void WriteTrack (int drive);

//===========================================================================
void CheckSpinning () {
  DWORD modechange = (floppymotoron && !floppy[currdrive].spinning);
  if (floppymotoron)
    floppy[currdrive].spinning = 20000;
  if (modechange)
    FrameRefreshStatus(DRAW_LEDS);
}

//===========================================================================
void GetImageTitle (LPCTSTR imagefilename, floppyptr fptr) {
  TCHAR   imagetitle[128];
  LPCTSTR startpos = imagefilename;

  // imagetitle = <FILENAME.EXT>
  if (_tcsrchr(startpos,TEXT('\\')))
    startpos = _tcsrchr(startpos,TEXT('\\'))+1;
  _tcsncpy(imagetitle,startpos,127);
  imagetitle[127] = 0;

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
  _tcsncpy(fptr->fullname,imagetitle,127);
  fptr->fullname[127] = 0;

  if (imagetitle[0])
  {
    LPTSTR dot = imagetitle;
    if (_tcsrchr(dot,TEXT('.')))
      dot = _tcsrchr(dot,TEXT('.'));
    if (dot > imagetitle)
      *dot = 0;
  }

  // fptr->imagename = <FILENAME> (ie. no extension)
  _tcsncpy(fptr->imagename,imagetitle,15);
  fptr->imagename[15] = 0;
}

//===========================================================================

static void AllocTrack(int drive)
{
  floppyptr fptr = &floppy[drive];
  fptr->trackimage = (LPBYTE)VirtualAlloc(NULL,NIBBLES_PER_TRACK,MEM_COMMIT,PAGE_READWRITE);
}

//===========================================================================
static void ReadTrack (int drive) {
  floppyptr fptr = &floppy[drive];
  if (fptr->track >= TRACKS) {
    fptr->trackimagedata = 0;
    return;
  }
  if (!fptr->trackimage)
    AllocTrack(drive);
  if (fptr->trackimage && fptr->imagehandle) {
    ImageReadTrack(fptr->imagehandle,
                   fptr->track,
                   fptr->phase,
                   fptr->trackimage,
                   &fptr->nibbles);
    fptr->byte           = 0;
    fptr->trackimagedata = (fptr->nibbles != 0);
  }
}

//===========================================================================
static void RemoveDisk (int drive) {
  floppyptr fptr = &floppy[drive];
  if (fptr->imagehandle) {
    if (fptr->trackimage && fptr->trackimagedirty)
      WriteTrack(drive);
    ImageClose(fptr->imagehandle);
    fptr->imagehandle = (HIMAGE)0;
  }
  if (fptr->trackimage) {
    VirtualFree(fptr->trackimage,0,MEM_RELEASE);
    fptr->trackimage     = NULL;
    fptr->trackimagedata = 0;
  }
}

//===========================================================================
static void WriteTrack (int drive) {
  floppyptr fptr = &floppy[drive];
  if (fptr->track >= TRACKS)
    return;
  if (fptr->trackimage && fptr->imagehandle)
    ImageWriteTrack(fptr->imagehandle,
                    fptr->track,
                    fptr->phase,
                    fptr->trackimage,
                    fptr->nibbles);
  fptr->trackimagedirty = 0;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void DiskBoot () {

  // THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
  // IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
  if (floppy[0].imagehandle && ImageBoot(floppy[0].imagehandle))
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
  floppyptr fptr = &floppy[currdrive];
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
void DiskDestroy () {
  RemoveDisk(0);
  RemoveDisk(1);
}

//===========================================================================
BYTE __stdcall DiskEnable (WORD, BYTE address, BYTE, BYTE, ULONG) {
  currdrive = address & 1;
  floppy[!currdrive].spinning   = 0;
  floppy[!currdrive].writelight = 0;
  CheckSpinning();
  return 0;
}

//===========================================================================
LPCTSTR DiskGetFullName (int drive) {
  return floppy[drive].fullname;
}

//===========================================================================
void DiskGetLightStatus (int *drive1, int *drive2) {
  *drive1 = floppy[0].spinning ? floppy[0].writelight ? 2
                                                      : 1
                               : 0;
  *drive2 = floppy[1].spinning ? floppy[1].writelight ? 2
                                                      : 1
                               : 0;
}

//===========================================================================
LPCTSTR DiskGetName (int drive) {
  return floppy[drive].imagename;
}

//===========================================================================
void DiskInitialize () {
  int loop = DRIVES;
  while (loop--)
    ZeroMemory(&floppy[loop],sizeof(floppyrec));
  TCHAR imagefilename[MAX_PATH];
  _tcscpy(imagefilename,progdir);
  _tcscat(imagefilename,TEXT("MASTER.DSK"));
  DiskInsert(0,imagefilename,0,0);
}

//===========================================================================
int DiskInsert (int drive, LPCTSTR imagefilename, BOOL writeprotected, BOOL createifnecessary) {
  floppyptr fptr = &floppy[drive];
  if (fptr->imagehandle)
    RemoveDisk(drive);
  ZeroMemory(fptr,sizeof(floppyrec));
  fptr->writeprotected = writeprotected;
  int error = ImageOpen(imagefilename,
                        &fptr->imagehandle,
                        &fptr->writeprotected,
                        createifnecessary);
  if (!error)
    GetImageTitle(imagefilename,fptr);
  return error;
}

//===========================================================================
BOOL DiskIsSpinning () {
  return floppymotoron;
}

//===========================================================================
void DiskNotifyInvalidImage (LPCTSTR imagefilename,int error) {
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
  MessageBox(framewindow,
             buffer,
             TITLE,
             MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}

//===========================================================================
BYTE __stdcall DiskReadWrite (WORD programcounter, BYTE, BYTE, BYTE, ULONG) {
  floppyptr fptr = &floppy[currdrive];
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
  ofn.hwndOwner       = framewindow;
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
  return MemReturnRandomData(floppy[currdrive].writeprotected);
}

//===========================================================================
BYTE __stdcall DiskSetWriteMode (WORD, BYTE, BYTE, BYTE, ULONG) {
  floppywritemode = 1;
  BOOL modechange = !floppy[currdrive].writelight;
  floppy[currdrive].writelight = 20000;
  if (modechange)
    FrameRefreshStatus(DRAW_LEDS);
  return MemReturnRandomData(1);
}

//===========================================================================
void DiskUpdatePosition (DWORD cycles) {
  int loop = 2;
  while (loop--) {
    floppyptr fptr = &floppy[loop];
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
	if(floppy[0].spinning || floppy[1].spinning)
		return false;

	// Swap disks between drives
	floppyrec fr;

	// Swap trackimage ptrs (so don't need to swap the buffers' data)
	memcpy(&fr, &floppy[0], sizeof(floppyrec));
	memcpy(&floppy[0], &floppy[1], sizeof(floppyrec));
	memcpy(&floppy[1], &fr, sizeof(floppyrec));

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
		strcpy(pSS->Unit[i].szFileName, floppy[i].fullname);
		pSS->Unit[i].track				= floppy[i].track;
		pSS->Unit[i].phase				= floppy[i].phase;
		pSS->Unit[i].byte				= floppy[i].byte;
		pSS->Unit[i].writeprotected		= floppy[i].writeprotected;
		pSS->Unit[i].trackimagedata		= floppy[i].trackimagedata;
		pSS->Unit[i].trackimagedirty	= floppy[i].trackimagedirty;
		pSS->Unit[i].spinning			= floppy[i].spinning;
		pSS->Unit[i].writelight			= floppy[i].writelight;
		pSS->Unit[i].nibbles			= floppy[i].nibbles;

		if(floppy[i].trackimage)
			memcpy(pSS->Unit[i].nTrack, floppy[i].trackimage, NIBBLES_PER_TRACK);
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

		ZeroMemory(&floppy[i], sizeof(floppyrec));
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

//		strcpy(floppy[i].fullname, pSS->Unit[i].szFileName);
		floppy[i].track				= pSS->Unit[i].track;
		floppy[i].phase				= pSS->Unit[i].phase;
		floppy[i].byte				= pSS->Unit[i].byte;
//		floppy[i].writeprotected	= pSS->Unit[i].writeprotected;
		floppy[i].trackimagedata	= pSS->Unit[i].trackimagedata;
		floppy[i].trackimagedirty	= pSS->Unit[i].trackimagedirty;
		floppy[i].spinning			= pSS->Unit[i].spinning;
		floppy[i].writelight		= pSS->Unit[i].writelight;
		floppy[i].nibbles			= pSS->Unit[i].nibbles;

		//

		if(!bImageError)
		{
			if((floppy[i].trackimage == NULL) && floppy[i].nibbles)
				AllocTrack(i);

			if(floppy[i].trackimage == NULL)
				bImageError = true;
			else
				memcpy(floppy[i].trackimage, pSS->Unit[i].nTrack, NIBBLES_PER_TRACK);
		}

		if(bImageError)
		{
			floppy[i].trackimagedata	= 0;
			floppy[i].trackimagedirty	= 0;
			floppy[i].nibbles			= 0;
		}
	}

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	return 0;
}
