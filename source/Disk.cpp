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
#pragma  hdrstop
#include "..\resource\resource.h"

static BYTE __stdcall DiskControlMotor (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskControlStepper (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskEnable (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskReadWrite (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskSetLatchValue (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskSetReadMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall DiskSetWriteMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

#define LOG_DISK_ENABLED 1

// __VA_ARGS__ not supported on MSVC++ .NET 7.x
#if (LOG_DISK_ENABLED) && !defined(_VC71)
	#define LOG_DISK(format, ...) LOG(format, __VA_ARGS__)
#else
	#define LOG_DISK(...)
#endif

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

static WORD      currdrive       = 0;
static BOOL      diskaccessed    = 0;
static Disk_t    g_aFloppyDisk[DRIVES];
static BYTE      floppylatch     = 0;
static BOOL      floppymotoron   = 0;
static BOOL      floppywritemode = 0;
static WORD      phases; // state bits for stepper magnet phases 0 - 3

static void CheckSpinning();
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
static BYTE __stdcall DiskControlMotor (WORD, WORD address, BYTE, BYTE, ULONG) {
  floppymotoron = address & 1;
  CheckSpinning();
  return MemReturnRandomData(1);
}

//===========================================================================
static BYTE __stdcall DiskControlStepper (WORD, WORD address, BYTE, BYTE, ULONG)
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
    int newtrack = MIN(TRACKS-1, fptr->phase >> 1); // (round half tracks down)
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
void DiskDestroy ()
{
	RemoveDisk(0);
	RemoveDisk(1);
}

//===========================================================================
static BYTE __stdcall DiskEnable (WORD, WORD address, BYTE, BYTE, ULONG) {
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

BYTE __stdcall Disk_IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall Disk_IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

void DiskInitialize ()
{
	int loop = DRIVES;
	while (loop--)
		ZeroMemory(&g_aFloppyDisk[loop],sizeof(Disk_t ));

	TCHAR imagefilename[MAX_PATH];
	_tcscpy(imagefilename,g_sProgramDir);
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
BOOL DiskIsSpinning ()
{
	return floppymotoron;
}

//===========================================================================
void DiskNotifyInvalidImage (LPCTSTR imagefilename,int error)
{
	TCHAR buffer[MAX_PATH+128];

	switch (error)
	{

	case 1:
		wsprintf(
			buffer,
			TEXT("Unable to open the file %s."),
			(LPCTSTR)imagefilename);
		break;

	case 2:
		wsprintf(
			buffer,
			TEXT("Unable to use the file %s\nbecause the ")
			TEXT("disk image format is not recognized."),
			(LPCTSTR)imagefilename);
		break;

	default:
		// IGNORE OTHER ERRORS SILENTLY
		return;
	}

	MessageBox(
		g_hFrameWindow,
		buffer,
		g_pAppTitle,
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
static BYTE __stdcall DiskReadWrite (WORD programcounter, WORD, BYTE, BYTE, ULONG) {
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
  if (0)
  {
    LOG_DISK("nib %4X = %2X\r", fptr->byte, result);
  }
  if (++fptr->byte >= fptr->nibbles)
    fptr->byte = 0;
  return result;
}

//===========================================================================

void DiskReset()
{
	floppymotoron = 0;
	phases = 0;
}

//===========================================================================
void DiskSelectImage (int drive, LPSTR pszFilename)
{
  TCHAR directory[MAX_PATH] = TEXT("");
  TCHAR filename[MAX_PATH];
  TCHAR title[40];

  strcpy(filename, pszFilename);

  RegLoadString(TEXT("Preferences"),REGVALUE_PREF_START_DIR,1,directory,MAX_PATH);
  _tcscpy(title,TEXT("Select Disk Image For Drive "));
  _tcscat(title,drive ? TEXT("2") : TEXT("1"));

  OPENFILENAME ofn;
  ZeroMemory(&ofn,sizeof(OPENFILENAME));
  ofn.lStructSize     = sizeof(OPENFILENAME);
  ofn.hwndOwner       = g_hFrameWindow;
  ofn.hInstance       = g_hInstance;
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
        RegSaveString(TEXT("Preferences"),REGVALUE_PREF_START_DIR,1,filename);
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

static BYTE __stdcall DiskSetLatchValue (WORD, WORD, BYTE write, BYTE value, ULONG) {
  if (write)
    floppylatch = value;
  return floppylatch;
}

//===========================================================================
static BYTE __stdcall DiskSetReadMode (WORD, WORD, BYTE, BYTE, ULONG) {
  floppywritemode = 0;
  return MemReturnRandomData(g_aFloppyDisk[currdrive].writeprotected);
}

//===========================================================================
static BYTE __stdcall DiskSetWriteMode (WORD, WORD, BYTE, BYTE, ULONG) {
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

static BYTE __stdcall Disk_IORead(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
static BYTE __stdcall Disk_IOWrite(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

void DiskLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	const UINT DISK2_FW_SIZE = 256;

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

	memcpy(pCxRomPeripheral + uSlot*256, pData, DISK2_FW_SIZE);

	// TODO/FIXME: HACK! REMOVE A WAIT ROUTINE FROM THE DISK CONTROLLER'S FIRMWARE
	*(pCxRomPeripheral+0x064C) = 0xA9;
	*(pCxRomPeripheral+0x064D) = 0x00;
	*(pCxRomPeripheral+0x064E) = 0xEA;

	//

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
